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

#include "win_tun.h"
#include "client.h"
#include "log.h"

using namespace flutter;
using namespace std;

namespace
{

  template <typename T = EncodableValue>
  class FyVpnSdkPluginStreamHandler : public StreamHandler<T>
  {

  public:
    FyVpnSdkPluginStreamHandler() = default;

    virtual ~FyVpnSdkPluginStreamHandler() = default;

    void send(int event)
    {
      if (uint64_t(this) == 0xddddddddddddddddul)
        return;

      if (m_sink.get())
      {
        m_sink.get()->Success(EncodableValue(event));
      }
    }

  protected:
    unique_ptr<StreamHandlerError<T> > OnListenInternal(const T *arguments, unique_ptr<EventSink<EncodableValue> > &&events) override
    {

      this->m_sink = move(events);

      win_tun_init();
#ifdef DEBUG
      fy_log_set_level(1);
#else
      fy_log_set_level(5);
#endif

      return nullptr;
    }

    unique_ptr<StreamHandlerError<T> > OnCancelInternal(const T *arguments) override
    {

      this->m_sink.release();

      win_tun_clean();

      return nullptr;
    }

  private:
    unique_ptr<EventSink<T> > m_sink;
  };

  class FyVpnSdkPlugin : public Plugin
  {
  public:
    win_tun_t *tun = NULL;
    static void RegisterWithRegistrar(PluginRegistrarWindows *registrar);

    FyVpnSdkPlugin();

    virtual ~FyVpnSdkPlugin();

    FyVpnSdkPluginStreamHandler<> *m_handler = nullptr;

    void send_event(int event);

  private:
    // Called when a method is called on this plugin"s channel from Dart.
    void HandleMethodCall(
        const MethodCall<EncodableValue> &method_call,
        unique_ptr<MethodResult<EncodableValue> > result);

    int start(int protocol, const char *ip, int port, const char *user_name, const char *password, const char *cert);

    int stop();

    fy_client_t *cli = NULL;

    int error = 0;
    int state = 0;
    thread cli_thread;
    thread win_tun_thread;
    // unique_ptr<EventSink<EncodableValue> > eventSinkPtr;
    unique_ptr<MethodChannel<EncodableValue> > m_method_channel;
    unique_ptr<EventChannel<EncodableValue> > m_event_channel;
  };

  static ssize_t _tun_read(win_tun_t *tun, uint8_t *buf, size_t length)
  {
    fy_client_t *cli = (fy_client_t *)win_tun_get_context(tun);

    if (length > cli->tun_mtu)
    {
      fy_log_error("client tun read length to larger, max : %d, read : %d",
                   cli->tun_mtu,
                   length);
    }
    else if (buf[0] >> 4 == 4) /* ipv4 */
    {
      fy_log_debug("client win tun read packet: ");

      fy_log_print_packet(buf, length);

      fy_client_tun_on_read(cli, buf, length);
    }
    else
    {
      fy_log_debug("unsupported packet type ,dropped ...");
    }

    return (ssize_t)length;
  }

  static void tun_run(win_tun_t *tun)
  {
    while (win_tun_alive(tun))
    {
      int ret = win_tun_send_readable_single(tun);

      if (ret < 0)
      {
        break;
      }
    }
  }

  static fy_return_code tun_config_ipv4(fy_client_t *cli, fy_network_config_ipv4_t *config_ipv4)
  {
    FyVpnSdkPlugin *plugin = (FyVpnSdkPlugin *)cli->data;

    plugin->tun = win_tun_create(config_ipv4->local_ip, config_ipv4->netmask, config_ipv4->dns, config_ipv4->mtu);

    if (!plugin->tun)
    {
      fy_log_error("config win tun failed !");

      return FY_ERR_CONFIG_IPV4;
    }

    //win tun start
    win_tun_set_on_read(plugin->tun, &_tun_read);

    cli->tun_mtu = config_ipv4->mtu;

    win_tun_set_context(plugin->tun, cli);

    win_tun_start(cli->loop, plugin->tun);

    plugin->win_tun_thread = thread(&tun_run, plugin->tun);

    return FY_SUCCESS;
  }

  static ssize_t tun_write(fy_client_t *cli, uint8_t *buf, size_t length)
  {
    FyVpnSdkPlugin *plugin = (FyVpnSdkPlugin *)cli->data;

    return win_tun_write(plugin->tun, buf, length);
  }

  fy_return_code state_on_change(fy_client_t *client, fy_state_e state)
  {

    FyVpnSdkPlugin *plugin = (FyVpnSdkPlugin *)client->data;

    plugin->send_event(fy_client_get_state_int(client));

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
    fy_client_run(client);
  }

  void FyVpnSdkPlugin::send_event(int event)
  {
    if (event >= 0)
    {
      this->state = event;
    }
    else
    {
      this->error = event;
    }

    this->m_handler->send(event);
  }

  int FyVpnSdkPlugin::stop()
  {
    if (this->tun)
    {
      win_tun_stop(this->tun);

      this->win_tun_thread.join();

      win_tun_destroy(this->tun);

      this->tun = NULL;
    }

    if (this->cli)
    {
      fy_client_stop(this->cli);

      this->cli_thread.join();

      fy_client_destroy(this->cli);

      this->cli = NULL;
    }

    return 0;
  }

  int FyVpnSdkPlugin::start(int protocol, const char *ip, int port, const char *user_name, const char *password, const char *cert)
  {
    this->stop();

    size_t cert_len = 0;

    if (cert)
    {
      cert_len = strlen(cert) + 1;
    }

    this->cli = fy_client_create((fy_conn_protocol_e)protocol, ip, port, user_name, password, (uint8_t *)cert, cert_len);

    if (this->cli)
    {

      this->cli->data = this;

      fy_client_set_config_ipv4_cb(this->cli, &tun_config_ipv4);

      fy_client_set_pier_on_read_cb(this->cli, &tun_write);

      fy_client_set_state_on_change_cb(this->cli, &state_on_change);

      fy_client_set_on_err_cb(this->cli, &on_error);

      this->cli_thread = thread(&worker_run, this->cli);
      // thread t(&worker_run, this->cli);
      // t.detach();

      return 0;
    }

    return -1;
  }

  // static
  void FyVpnSdkPlugin::RegisterWithRegistrar(
      PluginRegistrarWindows *registrar)
  {

    auto plugin = make_unique<FyVpnSdkPlugin>();

    plugin->m_method_channel =
        make_unique<MethodChannel<EncodableValue> >(
            registrar->messenger(), "fy_vpn",
            &StandardMethodCodec::GetInstance());

    plugin->m_method_channel->SetMethodCallHandler(
        [plugin_pointer = plugin.get()](const auto &call, auto result)
        {
          plugin_pointer->HandleMethodCall(call, move(result));
        });

    plugin->m_event_channel = make_unique<EventChannel<EncodableValue> >(
        registrar->messenger(), "fy_vpn_states",
        &StandardMethodCodec::GetInstance());

    plugin->m_handler = new FyVpnSdkPluginStreamHandler<>();

    auto _obj_stm_handle = static_cast<StreamHandler<EncodableValue> *>(plugin->m_handler);
    unique_ptr<StreamHandler<EncodableValue> > _ptr{_obj_stm_handle};

    plugin->m_event_channel->SetStreamHandler(move(_ptr));

    // eventChannel->SetStreamHandler(
    //     make_unique<StreamHandlerFunctions<EncodableValue> >(
    //         [plugin_pointer = plugin.get()](
    //             const EncodableValue *arguments,
    //             unique_ptr<EventSink<EncodableValue> > &&events)
    //         { return plugin_pointer->StreamHandleOnListen(arguments, std::move(events)); },
    //         [plugin_pointer = plugin.get()](const EncodableValue *arguments)
    //         {
    //           return plugin_pointer->StreamHandleOnCancel(arguments);
    //         }));

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
      result->Success(EncodableValue(win_tun_init() == 0));
    }
    else if (method_call.method_name().compare("getState") == 0)
    {
      int stateInt = fy_client_get_state_int(cli);

      //TODO c++ enum to int
      result->Success(EncodableValue(stateInt));
    }
    else if (method_call.method_name().compare("getError") == 0)
    {
      result->Success(EncodableValue(error));
    }
    else if (method_call.method_name().compare("start") == 0)
    {

      const auto *arguments = std::get_if<EncodableMap>(method_call.arguments());

      int protocol = 0;
      char *ip = NULL;
      int port = 0;
      char *user_name = NULL;
      char *password = NULL;
      char *cert = NULL;

      auto it = arguments->find(EncodableValue("PROTOCOL"));

      if (it != arguments->end())
      {
        string protocol_str = std::get<string>(it->second);

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
          protocol = 4; //FY_CONN_PROTOCOL_TLS; // 4
        }
        else if (protocol_str.compare("DTLS") == 0)
        {
          protocol = 3; //FY_CONN_PROTOCOL_DTLS; // 3
        }
        else
        {
          protocol = 3; //FY_CONN_PROTOCOL_DTLS;
        }
      }

      it = arguments->find(EncodableValue("SRV_IP"));

      if (it != arguments->end())
      {
        ip = (char *)std::get<string>(it->second).c_str();
      }

      it = arguments->find(EncodableValue("SRV_PORT"));

      if (it != arguments->end())
      {
        const char *port_str = std::get<string>(it->second).c_str();

        port = atoi(port_str);
      }

      it = arguments->find(EncodableValue("USER_NAME"));

      if (it != arguments->end())
      {
        user_name = (char *)std::get<string>(it->second).c_str();
      }

      it = arguments->find(EncodableValue("PASSWORD"));

      if (it != arguments->end())
      {
        password = (char *)std::get<string>(it->second).c_str();
      }

      it = arguments->find(EncodableValue("CERT"));

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
