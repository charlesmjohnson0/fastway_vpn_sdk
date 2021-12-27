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

#include <map>
#include <memory>
#include <sstream>

#include "client_win.h"

using flutter;
using std;

namespace
{

  class FyVpnSdkPlugin : public Plugin
  {
  public:
    static void RegisterWithRegistrar(PluginRegistrarWindows *registrar);

    FyVpnSdkPlugin();

    virtual ~FyVpnSdkPlugin();

  private:
    // Called when a method is called on this plugin's channel from Dart.
    void HandleMethodCall(
        const MethodCall<EncodableValue> &method_call,
        unique_ptr<MethodResult<EncodableValue> > result);

    unique_ptr<StreamHandlerError<EncodableValue>> StreamHandleOnListen(
          const EncodableValue *arguments,
          unique_ptr<EventSink<EncodableValue> > &&events))
    {

      eventSink = evetns;

      return NULL;
    }

    unique_ptr<StreamHandlerError<EncodableValue> > StreamHandleOnCancel(const EncodableValue *arguments)
    {

      // auto error = make_unique<StreamHandlerError<T> >(
      //     "error", "No OnCancel handler set", nullptr);
      return NULL;
    }

    fy_return_code state_on_change(fy_client_t *client, fy_state_e state)
    {
      this.state = state;

      if (eventSink)
      {
        eventSink.Success(EncodableValue(state));
      }

      return FY_SUCCESS;
    }

    fy_return_code on_error(fy_client_t *client, int err)
    {

      this.error = err;

      if (eventSink)
      {
        eventSink.Success(EncodableValue(err));
      }

      return FY_SUCCESS;
    }

    void *worker_run(void *arg)
    {
      fy_client_t *client = (fy_client_t *)arg;

      fy_run(client);

      fy_destroy(client);

      return NULL;
    }

    int start(int protocol, const char *ip, int port, const char *user_name, const char *password, const char *cert)
    {
      if (cli)
      {
        fy_stop(cli);
      }

      cli = fy_start(protocol, ip, port, user_name, password, cert);

      if (cli)
      {

        fy_set_on_state_change_cb(cli, state_on_change);

        fy_set_on_error_cb(cli, on_error);

        pthread_t thread;

        pthread_create(&thread, NULL, worker, (void *)cli);

        return 0;
      }

      return -1;
    }

    int stop()
    {
      return fy_stop(cli);
    }

    fy_client_t *cli;
    int error;
    int state;
    EventSink<EncodableValue> &&eventSink = 0;
  };

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

    auto plugin_pointer = plugin.get();

    eventChannel->SetStreamHandler(
        StreamHandlerFunctions(
            plugin_pointer->StreamHandleOnListen,
            plugin_pointer->StreamHandleOnCancel));

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
      UUID uuid;
      UuidCreate(&uuid);
      char *str;
      UuidToStringA(&uuid, (RPC_CSTR *)&str);
      result->Success(EncodableValue(str));
      RpcStringFreeA((RPC_CSTR *)&str);
    }
    else if ((method_call.method_name().compare("prepare") == 0) || (method_call.method_name().compare("prepared") == 0))
    {
      int res = fy_init();

      if (res == 0)
      {
        result->Success(EncodableValue(true));
      }
      else
      {
        result->Success(EncodableValue(false));
      }
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

      nodeMap = method_call.arguments;

      int protocol;
      const char *ip;
      int port;
      const char *user_name;
      const char *password;
      const char *cert;

      auto it = nodeMap.find('PROTOCOL');

      if (it != nodeMap.end())
      {
        protocol = it.second.LongValue();
      }

      it = nodeMap.find('SRV_IP');

      if (it != nodeMap.end())
      {
        ip = it.second.c_str();
      }

      it = nodeMap.find('SRV_PORT');

      if (it != nodeMap.end())
      {
        const char *port_str = it.second.c_str();

        port = atoi(port_str);
      }

      it = nodeMap.find('USER_NAME');

      if (it != nodeMap.end())
      {
        user_name = it.second.c_str();
      }

      it = nodeMap.find('PASSWORD');

      if (it != nodeMap.end())
      {
        password = it.second.c_str();
      }

      it = nodeMap.find('CERT');

      if (it != nodeMap.end())
      {
        cert = it.second.c_str();
      }

      int res = start(protocol, ip, port, user_name, password, cert);

      if (res == 0)
      {
        result->Success(EncodableValue(true));
      }
      else
      {
        result->Success(EncodableValue(false));
      }
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
