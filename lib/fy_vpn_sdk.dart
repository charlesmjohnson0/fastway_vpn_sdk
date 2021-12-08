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

extension fy_state_ext on fy_state {
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

enum fy_error {
  fy_err_connection,
  fy_err_auth_deny,
  fy_err_unknown,
}

extension fy_error_ext on fy_error {
  static fy_error valueOf(int index) {
    switch (index) {
      case -7:
        return fy_error.fy_err_auth_deny;
      case -102:
        return fy_error.fy_err_connection;
      default:
        return fy_error.fy_err_unknown;
    }
  }
}

class FyVpnSdk {
  FyVpnSdk._internal() {
    _eventChannel.receiveBroadcastStream().listen((event) {
      if (event > 0) {
        _stateController.sink.add(fy_state_ext.valueOf(event));
      } else if (event < 0) {
        _errorController.sink.add(fy_error_ext.valueOf(event));
      }
    });
  }

  static final FyVpnSdk _sdk = FyVpnSdk._internal();

  factory FyVpnSdk() {
    return _sdk;
  }

  static final StreamController<fy_state> _stateController =
      StreamController<fy_state>();

  static final StreamController<fy_error> _errorController =
      StreamController<fy_error>();

  static Stream<fy_error> get onError => _errorController.stream;
  static Stream<fy_state> get onStateChanged => _stateController.stream;

  static Future<fy_state> get state async {
    var state = await _channel.invokeMethod<int>('getState');
    return fy_state_ext.valueOf(state!);
  }

  static Future<String?> get platformVersion async {
    final String? version = await _channel.invokeMethod('getPlatformVersion');
    return version;
  }

  static Future<String> get deviceId async {
    final String deviceId = await _channel.invokeMethod('getDeviceId');
    return deviceId;
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
