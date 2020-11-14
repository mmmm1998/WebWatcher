#include "cefbrowserapp.hpp"

#include <cstring>

CefWindowlessBrowserApp::CefWindowlessBrowserApp() : CefApp(), m_renderHandler(new CefWindowlessRenderHandler())
{
}

CefWindowlessBrowserApp::~CefWindowlessBrowserApp()
{
}

CefRefPtr<CefRenderProcessHandler> CefWindowlessBrowserApp::GetRenderProcessHandler()
{
    return m_renderHandler;
}

void CefWindowlessRenderHandler::OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context)
{
}

void CefWindowlessRenderHandler::OnBrowserCreated(CefRefPtr<CefBrowser> browser)
{
}

CefRefPtr<CefBrowserProcessHandler> CefWindowlessBrowserApp::GetBrowserProcessHandler()
{
    return this;
}

bool CefWindowlessRenderHandler::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefProcessId source_process, CefRefPtr<CefProcessMessage> message)
{
    std::string messageName = message->GetName();
    //std::cout << "[Render] Got message from UI: " << messageName<< std::endl;
    if (messageName == "javascript")
    {
        CefRefPtr<CefListValue> args = message->GetArgumentList();
        std::string code = args->GetString(0);

        CefRefPtr<CefV8Value> retval;
        CefString script_url("unknown");
        CefRefPtr<CefV8Exception> exception;
        CefRefPtr<CefV8Context> context = frame->GetV8Context();

        bool hasError;
        CefString scriptResult;
        if (context->Eval(code, script_url, 0, retval, exception))
        {
            hasError = false;
            scriptResult = retval->IsString() ? retval->GetStringValue() : "js query return non-string value";
        }
        else
        {
            hasError = true;
            scriptResult = exception->GetMessage();
        }

        CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("javascript");
        CefRefPtr<CefListValue> outArgs = msg->GetArgumentList();
        outArgs->SetString(0, scriptResult);
        outArgs->SetBool(1, hasError);
        frame->SendProcessMessage(PID_BROWSER, msg);
    }

    return true;
}
