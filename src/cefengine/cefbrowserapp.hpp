#ifndef CEFBROWSERAPP_HPP
#define CEFBROWSERAPP_HPP

#include "include/cef_base.h"
#include "include/cef_app.h"
#include "include/cef_render_process_handler.h"

class CefWindowlessRenderHandler: public CefRenderProcessHandler
{
public:
    CefWindowlessRenderHandler(): CefRenderProcessHandler() {}

    // CefRenderProcessHandler methods
    bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId source_process, CefRefPtr<CefProcessMessage> message) override;
    virtual void OnBrowserCreated(CefRefPtr<CefBrowser> browser);
    virtual void OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) override;

    IMPLEMENT_REFCOUNTING(CefWindowlessRenderHandler);
};

// Handles the browser process related functionality
class CefWindowlessBrowserApp : public CefApp, public CefBrowserProcessHandler
{
public:
    CefWindowlessBrowserApp();
    virtual ~CefWindowlessBrowserApp();

    // CefApp methods
    virtual CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override;
    virtual CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override;

    IMPLEMENT_REFCOUNTING(CefWindowlessBrowserApp);

private:
    CefRefPtr<CefWindowlessRenderHandler> m_renderHandler;
};

#endif /* CEFBROWSERAPP_HPP */
