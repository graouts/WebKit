/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#import "DeprecatedGlobalValues.h"
#import "HTTPServer.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import <WebKit/WKFoundation.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>
#import <wtf/cocoa/NSURLExtras.h>
#import <wtf/cocoa/VectorCocoa.h>

static int provisionalLoadCount;

@interface LoadAlternateHTMLStringFromProvisionalLoadErrorController : NSObject <WKNavigationDelegate>
@end

@implementation LoadAlternateHTMLStringFromProvisionalLoadErrorController

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    if (error.code != NSURLErrorCancelled)
        [webView _loadAlternateHTMLString:@"error page" baseURL:nil forUnreachableURL:error.userInfo[NSURLErrorFailingURLErrorKey]];
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    isDone = true;
}

- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(WKNavigation *)navigation
{
    provisionalLoadCount++;
}

@end

static NSString *unloadableURL = @"data:";
static NSString *loadableURL = @"data:text/html,no%20error";

TEST(WKWebView, LoadAlternateHTMLStringFromProvisionalLoadError)
{
    auto webView = adoptNS([[WKWebView alloc] init]);
    auto controller = adoptNS([[LoadAlternateHTMLStringFromProvisionalLoadErrorController alloc] init]);
    [webView setNavigationDelegate:controller.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:unloadableURL]]];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:loadableURL]]];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    [webView goBack];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    WKBackForwardList *list = [webView backForwardList];
    EXPECT_EQ((NSUInteger)0, list.backList.count);
    EXPECT_EQ((NSUInteger)1, list.forwardList.count);
    EXPECT_STREQ([[list.forwardList.firstObject URL] absoluteString].UTF8String, loadableURL.UTF8String);
    EXPECT_STREQ([[list.currentItem URL] absoluteString].UTF8String, unloadableURL.UTF8String);

    EXPECT_TRUE([webView canGoForward]);
    if (![webView canGoForward])
        return;

    [webView goForward];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    EXPECT_EQ((NSUInteger)1, list.backList.count);
    EXPECT_EQ((NSUInteger)0, list.forwardList.count);
    EXPECT_STREQ([[list.backList.firstObject URL] absoluteString].UTF8String, unloadableURL.UTF8String);
    EXPECT_STREQ([[list.currentItem URL] absoluteString].UTF8String, loadableURL.UTF8String);
}

TEST(WKWebView, LoadAlternateHTMLStringFromProvisionalLoadErrorBackToBack)
{
    @autoreleasepool {
        RetainPtr<WKWebView> webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);

        RetainPtr<LoadAlternateHTMLStringFromProvisionalLoadErrorController> delegate = adoptNS([[LoadAlternateHTMLStringFromProvisionalLoadErrorController alloc] init]);
        [webView setNavigationDelegate:delegate.get()];

        char literal[] = "https://www.example.com<>/";
        NSURL* targetURL = WTF::URLWithData([NSData dataWithBytes:literal length:strlen(literal)], nil);
        [webView loadRequest:[NSURLRequest requestWithURL:targetURL]];
        [webView loadRequest:[NSURLRequest requestWithURL:targetURL]];

        isDone = false;
        TestWebKitAPI::Util::run(&isDone);
        // We should only start 1 provisional load for the _loadAlternateHTMLString.
        EXPECT_EQ(1, provisionalLoadCount);
    }
}

TEST(WKWebView, LoadAlternateHTMLStringNoFileSystemPath)
{
    auto webView = adoptNS([[WKWebView alloc] init]);
    [webView loadHTMLString:@"<html>hi</html>" baseURL:[NSURL URLWithString:@"file:///.file/id="]];
}

TEST(WKWebView, LoadNilAlternateHTMLStringDoesNotCrash)
{
    auto webView = adoptNS([[WKWebView alloc] init]);
    auto controller = adoptNS([LoadAlternateHTMLStringFromProvisionalLoadErrorController new]);
    [webView setNavigationDelegate:controller.get()];

    IGNORE_NULL_CHECK_WARNINGS_BEGIN
    [webView _loadAlternateHTMLString:nil baseURL:nil forUnreachableURL:[NSURL URLWithString:@"https://www.example.com"]];
    IGNORE_NULL_CHECK_WARNINGS_END

    TestWebKitAPI::Util::run(&isDone);
}

TEST(WKWebView, LoadAlternateHTMLStringFromProvisionalLoadErrorReload)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600)]);
    auto controller = adoptNS([[LoadAlternateHTMLStringFromProvisionalLoadErrorController alloc] init]);
    [webView setNavigationDelegate:controller.get()];

    NSURL *invalidURL = [NSURL URLWithString:@"https://www.example.com%3C%3E/"];
    [webView loadRequest:[NSURLRequest requestWithURL:invalidURL]];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    [webView reload];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    [webView reloadFromOrigin];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    [webView _reloadExpiredOnly];
    TestWebKitAPI::Util::run(&isDone);
    isDone = false;

    WKBackForwardList *list = [webView backForwardList];
    EXPECT_EQ((NSUInteger)0, list.backList.count);
}

TEST(WKWebView, LoadHTMLStringOrigin)
{
    using namespace TestWebKitAPI;
    bool done = false;
    HTTPServer server([&](Connection connection) {
        connection.receiveHTTPRequest([&](Vector<char>&& request) {
            EXPECT_TRUE(contains(request.span(), "Origin: custom-scheme://\r\n"_span));
            done = true;
        });
    });
    auto webView = adoptNS([WKWebView new]);
    auto delegate = adoptNS([TestNavigationDelegate new]);
    delegate.get().decidePolicyForNavigationAction = ^(WKNavigationAction *, void (^completionHandler)(WKNavigationActionPolicy)) {
        completionHandler(WKNavigationActionPolicyAllow);
    };
    webView.get().navigationDelegate = delegate.get();
    constexpr NSString *html = @"<script>var xhr = new XMLHttpRequest(); xhr.open('GET', 'http://127.0.0.1:%d/', true); xhr.send();</script>";
    [webView loadHTMLString:[NSString stringWithFormat:html, server.port()] baseURL:[NSURL URLWithString:@"custom-scheme://"]];
    Util::run(&done);
}

TEST(WebKit, LoadHTMLStringWithInvalidBaseURL)
{
    auto webView = adoptNS([[WKWebView alloc] initWithFrame:NSZeroRect]);

    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];

    __block bool didCrash = false;
    navigationDelegate.get().webContentProcessDidTerminate = ^(WKWebView *view, _WKProcessTerminationReason) {
        didCrash = true;
    };

    __block bool didFinishNavigation = false;
    navigationDelegate.get().didFinishNavigation = ^(WKWebView *view, WKNavigation *navigation) {
        didFinishNavigation = true;
    };

    [webView loadHTMLString:@"test" baseURL:[NSURL URLWithString:@"invalid"]];
    TestWebKitAPI::Util::run(&didFinishNavigation);
    EXPECT_WK_STREQ([webView URL].absoluteString, "");

    EXPECT_FALSE(didCrash);
}

#if !defined(NDEBUG)
TEST(Webkit, DISABLED_LoadMoreThan4GB)
#else
TEST(WebKit, LoadMoreThan4GB)
#endif
{
    constexpr auto html = "<script>"
    "async function main() {"
    "    const response = await fetch('/bigdata');"
    "    await response.arrayBuffer();"
    "}"
    "main().catch(e => {"
    "    alert('caught error ' + e);"
    "}).finally(() => {"
    "    alert('did not catch error');"
    "});"
    "</script>"_s;

    using namespace TestWebKitAPI;
    auto longData = makeDispatchData(Vector<uint8_t>(static_cast<size_t>(0x10000000), [] (size_t) {
        return static_cast<uint8_t>('a');
    }));

    HTTPServer server(HTTPServer::UseCoroutines::Yes, [&] (Connection connection) -> ConnectionTask { while (true) {
        auto request = co_await connection.awaitableReceiveHTTPRequest();
        auto path = HTTPServer::parsePath(request);
        if (path == "/"_s)
            co_await connection.awaitableSend(HTTPResponse(html).serialize());
        else {
            co_await connection.awaitableSend("HTTP/1.1 200 OK\r\nContent-type: application/octet-stream\r\n\r\n"_s);
            for (size_t i = 0; i < 16; ++i)
                co_await connection.awaitableSend(RetainPtr { longData.get() });
            connection.terminate();
        }
    } });

    RetainPtr webView = adoptNS([WKWebView new]);
    [webView loadRequest:server.request()];
    EXPECT_WK_STREQ([webView _test_waitForAlert], "caught error RangeError: Out of memory");
}
