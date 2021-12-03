import 'dart:async';
import 'dart:io';
import 'package:flutter/services.dart';

const _channel = MethodChannel('fy_vpn');
const _eventChannel = EventChannel('fy_vpn_states');

enum fy_state {
  NONE,
  DISCONNECTED,
  CONNECTING,
  ONLINE,
  DISCONNECTING,
  AUTHENTICATING,
  CONFIGURING,
  ERROR,
}

extension fy_state_extension on fy_state {
  static fy_state valueOf(int index) {
    switch (index) {
      case 1:
        return fy_state.DISCONNECTED;
      case 2:
        return fy_state.CONNECTING;
      case 3:
        return fy_state.ONLINE;
      case 4:
        return fy_state.DISCONNECTING;
      case 5:
        return fy_state.AUTHENTICATING;
      case 6:
        return fy_state.CONFIGURING;
      case 9:
        return fy_state.ERROR;
      case 0:
      default:
        return fy_state.NONE;
    }
  }
}

class FyVpnSdk {
  static Future<String?> get platformVersion async {
    final String? version = await _channel.invokeMethod('getPlatformVersion');
    return version;
  }

  static Stream<fy_state> get onStateChanged => _eventChannel
      .receiveBroadcastStream()
      .map((event) => fy_state_extension.valueOf(event));

  static Future<fy_state> get state async {
    var state = await _channel.invokeMethod<int>('getState');
    return fy_state_extension.valueOf(state!);
  }

  static Future<bool> prepare() async {
    if (!Platform.isAndroid) {
      return true;
    }

    var preapred = await _channel.invokeMethod('prepare');

    return preapred;
  }

  static Future<bool> prepared() async {
    if (!Platform.isAndroid) {
      return true;
    }

    var preapred = await _channel.invokeMethod('prepared');

    return preapred;
  }

  static Future<void> start(parametersMap) async {
    await _channel.invokeMethod('start', parametersMap);
  }

  static Future<void> stop() async {
    await _channel.invokeMethod('stop');
  }

  static Future<void> startVpnService(String protocol, String serverIp,
      int serverPort, String username, String password, String? cert) async {
    await start({
      'PROTOCOL': protocol,
      'SRV_IP': serverIp,
      'SRV_PORT': "$serverPort",
      'USER_NAME': username,
      'PASSWORD': password,
      'CERT': cert
    });
  }
}
