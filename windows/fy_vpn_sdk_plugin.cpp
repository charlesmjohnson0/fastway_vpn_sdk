#include "include/fy_vpn_sdk/fy_vpn_sdk_plugin.h"

// This must be included before many other Windows headers.
#include <windows.h>

// For getPlatformVersion; remove unless needed for your plugin implementation.
#include <VersionHelpers.h>

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <flutter/event_channel.h>
#include <flutter/event_sink.h>
#include <flutter/event_stream_handler.h>
#include <flutter/event_stream_handler_functions.h>

// #pragma comment(lib, "rpcrt4.lib")
// #include <rpcdce.h>

#include <map>
#include <memory>
#include <sstream>

#include <thread>

#include "client_win.h"

using namespace flutter;
using namespace std;

namespace
{

  fy_return_code state_on_change(fy_client_t *client, fy_state_e state);
  fy_return_code on_error(fy_client_t *client, int err);
  void worker_run(fy_client_t *client);

  class FyVpnSdkPlugin : public Plugin
  {
  public:
    static void RegisterWithRegistrar(PluginRegistrarWindows *registrar);

    FyVpnSdkPlugin();

    virtual ~FyVpnSdkPlugin();

    void send_event(int event)
    {
      if (event >= 0)
      {
        this->state = event;
      }
      else
      {
        this->error = event;
      }

      if (this->eventSinkPtr.get())
      {
        (this->eventSinkPtr.get())->Success(EncodableValue(event));
      }
    }

  private:
    // Called when a method is called on this plugin"s channel from Dart.
    void HandleMethodCall(
        const MethodCall<EncodableValue> &method_call,
        unique_ptr<MethodResult<EncodableValue> > result);

    unique_ptr<StreamHandlerError<EncodableValue> > StreamHandleOnListen(
        const EncodableValue *arguments,
        unique_ptr<EventSink<EncodableValue> > &&events)
    {

      this->eventSinkPtr = std::move(events);

      return nullptr;
    }

    unique_ptr<StreamHandlerError<EncodableValue> > StreamHandleOnCancel(const EncodableValue *arguments)
    {

      this->eventSinkPtr.release();

      return nullptr;
    }

    int start(int protocol, const char *ip, int port, const char *user_name, const char *password, const char *cert)
    {
      if (this->cli)
      {
        fy_stop(this->cli);
      }

      this->cli = fy_start(protocol, ip, port, user_name, password, cert);

      if (this->cli)
      {

        this->cli->data = this;

        fy_set_on_state_change_cb(this->cli, state_on_change);

        fy_set_on_error_cb(this->cli, on_error);

        thread t(&worker_run, this->cli);

        t.detach();

        return 0;
      }

      return -1;
    }

    int stop()
    {
      return fy_stop(this->cli);
    }

    fy_client_t *cli;
    int error = 0;
    int state = 0;
    unique_ptr<EventSink<EncodableValue> > eventSinkPtr;
  };

  fy_return_code state_on_change(fy_client_t *client, fy_state_e state)
  {

    FyVpnSdkPlugin *plugin = (FyVpnSdkPlugin *)client->data;

    plugin->send_event(fy_get_state(client));

    return FY_SUCCESS;
  }

  fy_return_code on_error(fy_client_t *client, int err)
  {

    FyVpnSdkPlugin *plugin = (FyVpnSdkPlugin *)client->data;

    plugin->send_event(err);

    return FY_SUCCESS;
  }

  void worker_run(fy_client_t *client)
  {
    // fy_client_t *client = (fy_client_t *)arg;

    fy_run(client);

    fy_destroy(client);
  }

  // static
  void FyVpnSdkPlugin::RegisterWithRegistrar(
      PluginRegistrarWindows *registrar)
  {
    auto channel =
        make_unique<MethodChannel<EncodableValue> >(
            registrar->messenger(), "fy_vpn_sdk",
            &StandardMethodCodec::GetInstance());

    auto plugin = make_unique<FyVpnSdkPlugin>();

    channel->SetMethodCallHandler(
        [plugin_pointer = plugin.get()](const auto &call, auto result)
        {
          plugin_pointer->HandleMethodCall(call, move(result));
        });

    auto eventChannel = make_unique<EventChannel<EncodableValue> >(
        registrar->messenger(), "fy_vpn_states",
        &StandardMethodCodec::GetInstance());

    eventChannel->SetStreamHandler(
        make_unique<StreamHandlerFunctions<EncodableValue> >(
            [plugin_pointer = plugin.get()](
                const EncodableValue *arguments,
                unique_ptr<EventSink<EncodableValue> > &&events)
            { return plugin_pointer->StreamHandleOnListen(arguments, std::move(events)); },
            [plugin_pointer = plugin.get()](const EncodableValue *arguments)
            {
              return plugin_pointer->StreamHandleOnCancel(arguments);
            }));

    registrar->AddPlugin(move(plugin));
  }

  FyVpnSdkPlugin::FyVpnSdkPlugin() {}

  FyVpnSdkPlugin::~FyVpnSdkPlugin() {}

  void FyVpnSdkPlugin::HandleMethodCall(
      const MethodCall<EncodableValue> &method_call,
      unique_ptr<MethodResult<EncodableValue> > result)
  {
    if (method_call.method_name().compare("getPlatformVersion") == 0)
    {
      ostringstream version_stream;
      version_stream << "Windows ";
      if (IsWindows10OrGreater())
      {
        version_stream << "10+";
      }
      else if (IsWindows8OrGreater())
      {
        version_stream << "8";
      }
      else if (IsWindows7OrGreater())
      {
        version_stream << "7";
      }
      result->Success(EncodableValue(version_stream.str()));
    }
    else if (method_call.method_name().compare("getDeviceId") == 0)
    {
      // UUID uuid;
      // UuidCreate(&uuid);
      // char *str;
      // UuidToStringA(&uuid, (RPC_CSTR *)&str);
      // result->Success(EncodableValue(str));
      // RpcStringFreeA((RPC_CSTR *)&str);

      result->Success(EncodableValue("windows"));
    }
    else if ((method_call.method_name().compare("prepare") == 0) || (method_call.method_name().compare("prepared") == 0))
    {
      result->Success(EncodableValue(fy_init() == 0));
    }
    else if (method_call.method_name().compare("getState") == 0)
    {
      result->Success(EncodableValue(fy_get_state(cli)));
    }
    else if (method_call.method_name().compare("getError") == 0)
    {
      result->Success(EncodableValue(error));
    }
    else if (method_call.method_name().compare("start") == 0)
    {

      const auto *arguments = std::get<EncodableMap>(method_call.arguments());

      int protocol = 0;
      char *ip = NULL;
      int port = 0;
      char *user_name = NULL;
      char *password = NULL;
      char *cert = NULL;

      auto it = arguments->find("PROTOCOL");

      if (it != arguments->end())
      {
        string &protocol_str = std::get<string>(it->second);

        if (protocol_str.compare("UDP") == 0)
        {
          protocol = 1; // FY_CONN_PROTOCOL_UDP; // 1
        }
        else if (protocol_str.compare("TCP") == 0)
        {
          protocol = 2; //FY_CONN_PROTOCOL_TCP; // 2
        }
        else if (protocol_str.compare("TLS") == 0)
        {
          protocol = 3; //FY_CONN_PROTOCOL_TLS; // 3
        }
        else if (protocol_str.compare("DTLS") == 0)
        {
          protocol = 4; //FY_CONN_PROTOCOL_DTLS; // 4
        }
        else
        {
          protocol = 4; //FY_CONN_PROTOCOL_DTLS;
        }
      }

      it = arguments->find("SRV_IP");

      if (it != arguments->end())
      {
        ip = (char *)std::get<string>(it->second).c_str();
      }

      it = arguments->find("SRV_PORT");

      if (it != arguments->end())
      {
        const char *port_str = std::get<string>(it->second).c_str();

        port = atoi(port_str);
      }

      it = arguments->find("USER_NAME");

      if (it != arguments->end())
      {
        user_name = (char *)std::get<string>(it->second).c_str();
      }

      it = arguments->find("PASSWORD");

      if (it != arguments->end())
      {
        password = (char *)std::get<string>(it->second).c_str();
      }

      it = arguments->find("CERT");

      if (it != arguments->end())
      {
        cert = (char *)std::get<string>(it->second).c_str();
      }

      int res = start(protocol, ip, port, user_name, password, cert);

      result->Success(EncodableValue(res == 0));
    }
    else if (method_call.method_name().compare("stop") == 0)
    {

      stop();

      result->Success(EncodableValue(true));
    }
    else
    {
      result->NotImplemented();
    }
  }

} // namespace

void FyVpnSdkPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar)
{
  FyVpnSdkPlugin::RegisterWithRegistrar(
      PluginRegistrarManager::GetInstance()
          ->GetRegistrar<PluginRegistrarWindows>(registrar));
}
