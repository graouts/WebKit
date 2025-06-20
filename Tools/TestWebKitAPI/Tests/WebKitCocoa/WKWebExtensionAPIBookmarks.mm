/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "HTTPServer.h"
#import "WebExtensionUtilities.h"
#import <WebKit/WKWebExtensionPermissionPrivate.h>

#if ENABLE(WK_WEB_EXTENSIONS_BOOKMARKS)

#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/_WKFeature.h>

namespace TestWebKitAPI {

#pragma mark - Constants

static constexpr auto *bookmarkOffManifest = @{
    @"manifest_version": @3,
    @"name": @"bookmarkpermission off Test",
    @"description": @"bookmarkpermission off Test",
    @"version": @"1",

    @"permissions": @[],
    @"background": @{
        @"scripts": @[ @"background.js", ],
        @"type": @"module",
        @"persistent": @NO,
    },
};

static auto *bookmarkOnManifest = @{
    @"manifest_version": @3,
    @"name": @"bookmarkpermission on Test",
    @"description": @"bookmarkpermission on Test",
    @"version": @"1",

    @"permissions": @[ @"bookmarks" ],
    @"background": @{
        @"scripts": @[ @"background.js", ],
        @"type": @"module",
        @"persistent": @NO,
    },
    @"action": @{
        @"default_title": @"Test Action",
        @"default_popup": @"popup.html",
    },
};

#pragma mark - Test Fixture

class WKWebExtensionAPIBookmarks : public testing::Test {
protected:
    WKWebExtensionAPIBookmarks()
    {
        bookmarkConfig = WKWebExtensionControllerConfiguration.nonPersistentConfiguration;
        if (!bookmarkConfig.webViewConfiguration)
            bookmarkConfig.webViewConfiguration = [[WKWebViewConfiguration alloc] init];

        for (_WKFeature *feature in [WKPreferences _features]) {
            if ([feature.key isEqualToString:@"WebExtensionBookmarksEnabled"])
                [bookmarkConfig.webViewConfiguration.preferences _setEnabled:YES forFeature:feature];
        }
    }
    RetainPtr<TestWebExtensionManager> getManagerFor(NSArray<NSString *> *script, NSDictionary<NSString *, id> *manifest)
    {
        return getManagerFor(@{ @"background.js" : Util::constructScript(script) }, manifest);
    }

    RetainPtr<TestWebExtensionManager> getManagerFor(NSDictionary<NSString *, id> *resources, NSDictionary<NSString *, id> *manifest)
    {
        return Util::parseExtension(manifest, resources, bookmarkConfig);
    }

    WKWebExtensionControllerConfiguration *bookmarkConfig;
};

#pragma mark - Common Tests

TEST_F(WKWebExtensionAPIBookmarks, APISUnavailableWhenManifestDoesNotRequest)
{
    auto *script = @[
        @"browser.test.assertDeepEq(browser.bookmarks, undefined)",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(bookmarkOffManifest, @{ @"background.js": Util::constructScript(script) }, bookmarkConfig);
}

#pragma mark - more Tests

TEST_F(WKWebExtensionAPIBookmarks, APIAvailableWhenManifestRequests)
{
    auto *script = @[
        @"browser.test.assertFalse(browser.bookmarks === undefined)",
        @"browser.test.notifyPass()",
    ];

    Util::loadAndRunExtension(bookmarkOnManifest, @{ @"background.js": Util::constructScript(script) }, bookmarkConfig);
}

}

#endif

