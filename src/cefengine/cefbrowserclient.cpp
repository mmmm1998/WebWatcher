#include "cefbrowserclient.hpp"

#include "include/cef_app.h"

CefWindowlessBrowserClient::CefWindowlessBrowserClient()
{
}

CefRefPtr<CefLifeSpanHandler> CefWindowlessBrowserClient::GetLifeSpanHandler()
{
    return this;
}

CefRefPtr<CefRenderHandler> CefWindowlessBrowserClient::GetRenderHandler()
{
    return this;
}

CefWindowlessBrowserClient::~CefWindowlessBrowserClient()
{
}

bool CefWindowlessBrowserClient::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId source_process, CefRefPtr<CefProcessMessage> message)
{
    std::string messageName = message->GetName();
    //std::cout << "[Browser] Got message from render: " << messageName << std::endl;
    if (messageName == "javascript")
    {
        CefRefPtr<CefListValue> args = message->GetArgumentList();

        m_scriptResult = args->GetString(0);
        m_hasException = args->GetBool(1);
        m_haveResult = true;
    }

    return true;
}

bool CefWindowlessBrowserClient::haveScriptException()
{
    return m_hasException;
}

std::string CefWindowlessBrowserClient::getScriptResult()
{
    return m_scriptResult;
}

bool CefWindowlessBrowserClient::haveScriptResult()
{
    return m_haveResult;
}

void CefWindowlessBrowserClient::resetScriptResultHaving()
{
    m_haveResult = false;

    m_scriptResult = "NORESULT";
    m_hasException = false;
}

// Called after the browser client is succesfully created
void CefWindowlessBrowserClient::OnAfterCreated(CefRefPtr<CefBrowser> browser)
{
    m_browser = browser;
}

// Called before the browser is shutdown
void CefWindowlessBrowserClient::OnBeforeClose(CefRefPtr<CefBrowser> browser)
{
    m_browser = nullptr;
}

CefRefPtr<CefBrowser> CefWindowlessBrowserClient::GetBrowser()
{
    return m_browser;
}

// Called everytime there is something to paint to a "page". This is ignored, for drawing to e.g. offscreen bitmap, implement the logic here
void CefWindowlessBrowserClient::OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type, const RectList& dirtyRects, const void* buffer, int width, int height)
{
}

void CefWindowlessBrowserClient::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect)
{
    rect.Set(0, 0, kPseudoWindowWidth, kPseudoWindowHeight);
};
