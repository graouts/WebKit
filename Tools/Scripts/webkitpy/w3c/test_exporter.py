# Copyright (c) 2017, Apple Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL APPLE INC. AND ITS CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""
 This script uploads changes made to W3C web-platform-tests tests.
"""

import argparse
import logging
import os
import json

from webkitcorepy import string_utils
from webkitbugspy import Tracker
from webkitscmpy import local

from webkitpy.common.checkout.scm.git import Git
from webkitpy.common.host import Host
from webkitpy.common.net.bugzilla import Bugzilla
from webkitpy.common.webkit_finder import WebKitFinder
from webkitpy.w3c.wpt_github import WPTGitHub
from webkitpy.w3c.wpt_linter import WPTLinter
from webkitpy.w3c.common import WPT_GH_ORG, WPT_GH_REPO_NAME, WPT_GH_URL, WPTPaths
from webkitpy.common.memoized import memoized

from urllib.error import HTTPError

_log = logging.getLogger(__name__)

WEBKIT_WPT_DIR = 'LayoutTests/imported/w3c/web-platform-tests'
WPT_PR_URL = f'{WPT_GH_URL}/pull/'
WEBKIT_EXPORT_PR_LABEL = 'webkit-export'

EXCLUDED_FILE_SUFFIXES = ['-expected.txt', '-expected.html', '-expected-mismatch.html', '.worker.html', '.any.html', '.any.worker.html', '.any.serviceworker.html', '.any.sharedworker.html', 'w3c-import.log']


class WebPlatformTestExporter(object):
    def __init__(self, host, options, gitClass=Git, bugzillaClass=Bugzilla, WPTGitHubClass=WPTGitHub, WPTLinterClass=WPTLinter, repository=None):
        self._host = host
        self._filesystem = host.filesystem
        self._options = options

        self._host.initialize_scm()

        self._WPTGitHubClass = WPTGitHubClass
        self._gitClass = gitClass  # FIXME: Move git operations to webkitscmpy
        self._bugzilla = bugzillaClass()
        self._bug_id = options.bug_id
        self._bug = None

        repo_path = WebKitFinder(self._filesystem).webkit_base()
        self._repository = repository or local.Git(repo_path)

        issue = None
        if not self._bug_id:
            if options.attachment_id:
                self._bug_id = self._bugzilla.bug_id_for_attachment_id(options.attachment_id)  # FIXME: Dependency on webkitpy.bugzilla should be removed when patch workflow is disabled
            elif options.git_commit:
                self._bug_id = self._host.checkout().bug_id_for_this_commit(options.git_commit)

        if Tracker.instance() and (isinstance(self._bug_id, int) or string_utils.decode(self._bug_id).isnumeric()):
            issue = Tracker.instance().issue(int(self._bug_id))
        else:
            issue = Tracker.from_string(self._bug_id)
        if issue:
            self._bug_id = issue.id
            self._bug = issue

        if not self._options.repository_directory:
            webkit_finder = WebKitFinder(self._filesystem)
            self._options.repository_directory = WPTPaths.wpt_checkout_path(webkit_finder)

        self._linter = WPTLinterClass(self._options.repository_directory, host.filesystem)

        self._commit_message = options.message
        if not self._commit_message:
            self._commit_message = f'WebKit export of {issue.link}' if issue else 'Export made from a WebKit repository'

    @property
    def username(self):
        if hasattr(self, '_username'):
            return self._username

        self._ensure_username_and_token(self._options)
        return self._username

    @property
    def token(self):
        if hasattr(self, '_token'):
            return self._token

        self._ensure_username_and_token(self._options)
        return self._token

    @property
    @memoized
    def _github(self):
        return self._WPTGitHubClass(self._host, self.username, self.token) if self.username and self.token else None

    @property
    @memoized
    def _wpt_fork_branch_github_url(self):
        return f'https://github.com/{self.username}/{WPT_GH_REPO_NAME}/tree/{self._public_branch_name}'

    @property
    @memoized
    def _wpt_fork_remote(self):
        wpt_fork_remote = self._options.repository_remote
        if not wpt_fork_remote:
            wpt_fork_remote = self.username

        return wpt_fork_remote

    @property
    @memoized
    def _wpt_fork_push_url(self):
        wpt_fork_push_url = self._options.repository_remote_url
        if not wpt_fork_push_url:
            wpt_fork_push_url = f'https://{self.username}@github.com/{self.username}/{WPT_GH_REPO_NAME}.git'

        return wpt_fork_push_url

    @property
    @memoized
    def _git(self):
        return self._ensure_wpt_repository(f'{WPT_GH_URL}.git', self._options.repository_directory, self._gitClass)

    @property
    @memoized
    def _branch_name(self):
        return self._ensure_new_branch_name()

    @property
    @memoized
    def _public_branch_name(self):
        options = self._options
        return options.public_branch_name if options.public_branch_name else self._branch_name

    @property
    @memoized
    def _wpt_patch(self):
        patch_data = self._host.scm().create_patch(self._options.git_commit, [WEBKIT_WPT_DIR], commit_message=False) or b''
        patch_data = self._strip_ignored_files_from_diff(patch_data)
        if b'diff' not in patch_data:
            return ''
        return patch_data

    def has_wpt_changes(self):
        return bool(self._wpt_patch)

    def _find_filename(self, line):
        return line.split(b' ')[-1][2:]

    def _is_ignored_file(self, filename):
        filename = string_utils.decode(filename, target_type=str)
        if not filename.startswith(WEBKIT_WPT_DIR):
            return True
        for suffix in EXCLUDED_FILE_SUFFIXES:
            if filename.endswith(suffix):
                return True
        return False

    def _strip_ignored_files_from_diff(self, diff):
        lines = diff.split(b'\n')
        include_file = True
        new_lines = []
        for line in lines:
            if line.startswith(b'diff'):
                include_file = True
                filename = self._find_filename(line)
                if self._is_ignored_file(filename):
                    include_file = False
            if include_file:
                new_lines.append(line)

        return b'\n'.join(new_lines) + b'\n'

    def write_git_patch_file(self):
        _, patch_file = self._filesystem.open_binary_tempfile('wpt_export_patch')
        patch_data = self._wpt_patch
        if b'diff' not in patch_data:
            _log.info(f'No changes to upstream, patch data is: "{string_utils.decode(patch_data, target_type=str)}"')
            return b''
        # FIXME: We can probably try to use --relative git parameter to not do that replacement.
        patch_data = patch_data.replace(string_utils.encode(WEBKIT_WPT_DIR) + b'/', b'')

        # FIXME: Support stripping of <!-- webkit-test-runner --> comments.
        self.has_webkit_test_runner_specific_changes = b'webkit-test-runner' in patch_data
        if self.has_webkit_test_runner_specific_changes:
            _log.warning("Patch contains webkit-test-runner specific changes, please remove them before creating a PR")
            return b''

        self._filesystem.write_binary_file(patch_file, patch_data)
        return patch_file

    def _prompt_for_token(self, options):
        if options.non_interactive:
            return None
        return self._host.user.prompt_password('Enter GitHub OAuth token (or empty string to skip creating a pull request): ')

    def _prompt_for_username(self, options):
        if options.non_interactive:
            return None
        return self._host.user.prompt('Enter your GitHub username: ')

    def _ensure_username_and_token(self, options):
        self._username = options.username
        if not self._username:
            # FIXME: Use the keychain to store username and oauth token instead of .git/config
            self._username = self._git.local_config('github.username').rstrip()
            if not self._username:
                self._username = os.environ.get('GITHUB_USERNAME')
            if not self._username:
                self._username = self._prompt_for_username(options)
            if not self._username:
                raise ValueError("Missing GitHub username, please provide it as a command argument (see help for the command).")

        self._token = options.token
        if not self._token:
            self._token = self._git.local_config('github.token').rstrip()
            if not self._token:
                self._token = os.environ.get('GITHUB_TOKEN')
            if not self._token:
                self._token = self._prompt_for_token(options)
            if not self._token:
                _log.info(f"Missing GitHub token, the script will not be able to create a pull request to {WPT_GH_ORG}'s {WPT_GH_REPO_NAME} repository.")

        if self._token:
            self._validate_and_save_token(self._username, self._token)

    def _validate_and_save_token(self, username, token):
        url = 'https://api.github.com/user'
        headers = {'Accept': 'application/vnd.github.v3+json', 'Authorization': f'token {token}'}
        try:
            response = self._host.web.request(method='GET', url=url, data=None, headers=headers)
        except HTTPError:
            raise Exception("OAuth token is not valid")
        data = json.load(response)
        login = data.get('login', None)
        if login != username:
            raise Exception(f'OAuth token does not match the provided username. Provided user: {username}, github login: {login}')
        else:
            # Username and token are valid. Save them in the git config so we
            # do not need to ask for them again
            if not self._git.local_config('github.token'):
                self._git.set_local_config('github.token', token)
            if not self._git.local_config('github.username'):
                self._git.set_local_config('github.username', username)

    def _ensure_wpt_repository(self, url, wpt_repository_directory, gitClass):
        if not self._filesystem.exists(wpt_repository_directory):
            _log.info(f'Cloning {url} into {wpt_repository_directory}...')
            gitClass.clone(url, wpt_repository_directory, self._host.executive)
        git = gitClass(wpt_repository_directory, None, executive=self._host.executive, filesystem=self._filesystem)
        return git

    def _fetch_wpt_repository(self):
        _log.info('Fetching web-platform-tests repository')
        self._git.fetch()

    def _ensure_new_branch_name(self):
        branch_name_prefix = "wpt-export-for-webkit-" + (str(self._bug_id) if self._bug_id else "0")
        branch_name = branch_name_prefix
        counter = 0
        while self._git.branch_ref_exists(branch_name):
            branch_name = (f'{branch_name_prefix}-{counter!s}')
            counter = counter + 1
        return branch_name

    def clean(self):
        _log.info('Cleaning web-platform-tests master branch')
        self._git.checkout('master')
        self._git.reset_hard('origin/master')

    def create_branch_with_patch(self, patch):
        _log.info('Applying patch to web-platform-tests branch ' + self._branch_name)
        try:
            self._git.checkout_new_branch(self._branch_name)
        except Exception as e:
            _log.warning(e)
            _log.info("Retrying to create the branch")
            self._git.delete_branch(self._branch_name)
            self._git.checkout_new_branch(self._branch_name)
        try:
            self._git.apply_mail_patch([patch, '-3'])
        except Exception as e:
            _log.warning(e)
            return False
        self._git.commit(['-a', '-m', self._commit_message])
        return True

    def push_to_wpt_fork(self):
        self.create_upload_remote_if_needed()
        _log.info('Pushing branch ' + self._branch_name + " to " + self._git.remote(["get-url", self._wpt_fork_remote]).rstrip())
        _log.info('This may take some time')
        self._git.push([self._wpt_fork_remote, self._branch_name + ":" + self._public_branch_name, '-f'])
        _log.info('Branch available at ' + self._wpt_fork_branch_github_url)
        return True

    def make_pull_request(self):
        if self.has_webkit_test_runner_specific_changes:
            _log.error('Cannot create a WPT PR since it contains webkit test runner specific changes')
            return

        if not self._github:
            _log.info('Skipping pull request because OAuth token was not provided. You can open the pull request manually using the branch ' + self._wpt_fork_branch_github_url)
            return

        _log.info('Making pull request')
        title = self._bug.title.replace("[", "\\[").replace("]", "\\]")
        # NOTE: this should contain the exact string "WebKit export" to match the condition in
        # https://github.com/web-platform-tests/wpt-pr-bot/blob/f53e625c4871010277dc68336b340b5cd86e2a10/lib/metadata/index.js#L87
        description = f'WebKit export from bug: [{title}]({self._bug.link})'
        pr_number = self.create_wpt_pull_request(self._wpt_fork_remote + ':' + self._public_branch_name, self._commit_message, description)
        if pr_number:
            try:
                self._github.add_label(pr_number, WEBKIT_EXPORT_PR_LABEL)
            except Exception as e:
                _log.warning(e)
                _log.info(f'Could not add label "{WEBKIT_EXPORT_PR_LABEL}" to pr #{pr_number}. User "{self.username}" may not have permission to update labels in the {WPT_GH_ORG}/{WPT_GH_REPO_NAME} repo.')
        if self._bug_id and pr_number:
            pr_url = f'{WPT_PR_URL}{pr_number}'
            self._bug.add_related_links([pr_url])
            self._bug.add_comment(f'Submitted web-platform-tests pull request: {pr_url}')

    def create_wpt_pull_request(self, remote_branch_name, title, body):
        pr_number = None
        try:
            pr_number = self._github.create_pr(remote_branch_name, title, body)
        except HTTPError as e:
            if e.code == 422:
                _log.info(f'Unable to create a new pull request for branch "{remote_branch_name}" because a pull request already exists. The branch has been updated and there is no further action needed.')
            else:
                _log.warning(e)
                _log.info('Error creating a pull request on github. Please ensure that the provided github token has the "public_repo" scope.')
        except Exception as e:
            _log.warning(e)
            _log.info('Error creating a pull request on github. Please ensure that the provided github token has the "public_repo" scope.')
        return pr_number

    def delete_local_branch(self, *, is_success=True):
        if self._options.clean and (is_success or self._options.clean_on_failure):
            _log.info('Removing local branch ' + self._branch_name)
            self._git.checkout('master')
            self._git.delete_branch(self._branch_name)
        else:
            _log.info('Keeping local branch ' + self._branch_name)

    def create_upload_remote_if_needed(self):
        if not self._wpt_fork_remote in self._git.remote([]):
            self._git.remote(["add", self._wpt_fork_remote, self._wpt_fork_push_url])

    def do_export(self):
        git_patch_file = self.write_git_patch_file()

        if not git_patch_file:
            _log.error("Unable to create a patch to apply to web-platform-tests repository")
            return

        self._fetch_wpt_repository()
        self.clean()

        if not self.create_branch_with_patch(git_patch_file):
            _log.error(f'Cannot create web-platform-tests local branch from the patch {git_patch_file!r}')
            self.delete_local_branch(is_success=False)
            return

        if git_patch_file and self.clean:
            self._filesystem.remove(git_patch_file)

        if self._options.use_linter:
            lint_errors = self._linter.lint()
            if lint_errors:
                _log.error(f'The wpt linter detected {lint_errors} linting error(s). Please address the above errors before attempting to export changes to the web-platform-test repository.')
                self.delete_local_branch(is_success=False)
                return

        try:
            if self.push_to_wpt_fork():
                if self._options.create_pull_request:
                    self.make_pull_request()
        except Exception:
            self.delete_local_branch(is_success=False)
            raise
        else:
            self.delete_local_branch(is_success=True)
        finally:
            _log.info("Finished")


def parse_args(args):
    description = f"""Script to generate a pull request to W3C web-platform-tests repository
    'Tools/Scripts/export-w3c-test-changes -c -g HEAD -b XYZ' will do the following:
    - Clone web-platform-tests repository if not done already and set it up for pushing branches.
    - Gather WebKit bug id XYZ bug and changes to apply to web-platform-tests repository based on the HEAD commit
    - Create a remote branch named webkit-XYZ on https://github.com/USERNAME/{WPT_GH_REPO_NAME}.git repository based on the locally applied patch.
       * USERNAME may be set using the environment variable GITHUB_USERNAME or as a command line option. It is then stored in git config as github.username.
       * {WPT_GH_URL}.git should have already been cloned to https://github.com/USERNAME/{WPT_GH_REPO_NAME}.git.
       * Github credential may be set using the environment variable GITHUB_TOKEN or as a command line option.
         (Please provide a valid GitHub 'Personal access token' with 'repo' as scope, it may be generated at https://github.com/settings/tokens).
         It is then stored in git config as github.token.
    - Make the related pull request on {WPT_GH_URL}.git repository.
    - Clean the local Git repository
    Notes:
    - It is safer to provide a bug id using -b option (bug id from a git commit is not always working).
    - As a dry run, one can start by running the script without -c. This will only create the branch on the user public GitHub repository.
    - By default, the script will create an https remote URL that will require a password-based authentication to GitHub. If you are using an SSH key, please use the --remote-url option.
    FIXME:
    - The script is not yet able to update an existing pull request.
    - Need a way to monitor the progress of the pull request so that status of all pending pull requests can be done at import time.
    """
    parser = argparse.ArgumentParser(prog='export-w3c-test-changes ...', description=description, formatter_class=argparse.RawDescriptionHelpFormatter)

    parser.add_argument('-g', '--git-commit', dest='git_commit', default=None, help='Git commit to apply')
    parser.add_argument('-b', '--bug', dest='bug_id', default=None, help='Bug ID or URL to search for patch')
    parser.add_argument('-a', '--attachment', dest='attachment_id', default=None, help='Attachment ID to search for patch')
    parser.add_argument('-n', '--name', dest='username', default=None, help='github user name if GITHUB_USERNAME is not defined or github.username in the WPT repo config is not defined')
    parser.add_argument('-t', '--token', dest='token', default=None, help='github token, needed for creating pull requests only if GITHUB_TOKEN env variable is not defined or github.token in the WPT repo config is not defined')
    parser.add_argument('-bn', '--branch-name', dest='public_branch_name', default=None, help='Branch name to push to')
    parser.add_argument('-m', '--message', dest='message', default=None, help='Commit message')
    parser.add_argument('-r', '--remote', dest='repository_remote', default=None, help='repository origin to use to push')
    parser.add_argument('-u', '--remote-url', dest='repository_remote_url', default=None, help='repository url to use to push')
    parser.add_argument('-d', '--repository', dest='repository_directory', default=None, help='repository directory')
    parser.add_argument('-c', '--create-pr', dest='create_pull_request', action='store_true', default=False, help='create pull request to w3c web-platform-tests')
    parser.add_argument('--non-interactive', action='store_true', dest='non_interactive', default=False, help='Never prompt the user, fail as fast as possible.')
    parser.add_argument('--no-linter', action='store_false', dest='use_linter', default=True, help='Disable linter.')
    parser.add_argument('--no-clean', action='store_false', dest='clean', help='Do not clean up.')
    parser.add_argument('--clean-on-failure', action='store_true', dest='clean_on_failure', help='Do not clean up on failure.')

    options, args = parser.parse_known_args(args)

    return options


def configure_logging():
    class LogHandler(logging.StreamHandler):

        def format(self, record):
            if record.levelno > logging.INFO:
                return f'{record.levelname}: {record.getMessage()}'
            return record.getMessage()

    logger = logging.getLogger('webkitpy.w3c.test_exporter')
    logger.propagate = False
    logger.setLevel(logging.INFO)
    handler = LogHandler()
    handler.setLevel(logging.INFO)
    logger.addHandler(handler)
    return handler


def main(_argv, _stdout, _stderr):
    options = parse_args(_argv)

    configure_logging()
    test_exporter = WebPlatformTestExporter(Host(), options)

    if not test_exporter.has_wpt_changes():
        _log.info('No changes to upstream. Exiting...')
        return

    test_exporter.do_export()
