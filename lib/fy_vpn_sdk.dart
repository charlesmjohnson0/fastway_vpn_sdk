import 'dart:async';
import 'dart:io';
import 'package:flutter/material.dart';
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
  fy_err_out_of_memory,
  fy_err_internel,
  fy_err_connection,
  fy_err_offline,
  fy_err_tls,
  fy_err_tun,
  fy_err_auth_deny,
  fy_err_unknown,
}

extension fy_error_ext on fy_error {
  static fy_error valueOf(int index) {
    switch (index) {
      case -1:
        return fy_error.fy_err_out_of_memory;
      case -7:
        return fy_error.fy_err_auth_deny;
      case -2:
      case -3:
      case -4:
      case -5:
      case -6:
      case -8:
        return fy_error.fy_err_internel;
      case -100:
      case -101:
      case -102:
      case -103:
      case -104:
        return fy_error.fy_err_connection;
      case -105:
        return fy_error.fy_err_offline;
      case -200:
      case -201:
        return fy_error.fy_err_tls;
      case -400:
      case -401:
      case -402:
      case -403:
      case -404:
        return fy_error.fy_err_tun;
      default:
        return fy_error.fy_err_unknown;
    }
  }
}

class FyVpnSdk {
  static final FyVpnSdk _sdk = FyVpnSdk._internal();

  factory FyVpnSdk() {
    return _sdk;
  }

  StreamSubscription? eventSubscription;

  FyVpnSdk._internal() {
    debugPrint(' sdk init ...');

    Future.delayed(const Duration(seconds: 2), () {
      eventSubscription =
          _eventChannel.receiveBroadcastStream().listen((event) {
        debugPrint(' event channel recve : $event');
        if (event > 0) {
          _stateController.sink.add(fy_state_ext.valueOf(event));
        } else if (event < 0) {
          _errorCode = event;
          _errorController.sink.add(fy_error_ext.valueOf(event));
        }
        // _eventController.sink.add(event);
      });
    });
  }

  final StreamController<fy_state> _stateController =
      StreamController<fy_state>.broadcast();

  final StreamController<fy_error> _errorController =
      StreamController<fy_error>.broadcast();

  // final StreamController<int> _eventController =
  //     StreamController<int>.broadcast();

  int _errorCode = 0;

  int get errorCode => _errorCode;
  Stream<fy_error> get onError => _errorController.stream;
  Stream<fy_state> get onStateChanged => _stateController.stream;
  // Stream<int> get onEvent => _eventController.stream;

  Future<fy_state> get state async {
    var state = await _channel.invokeMethod<int>('getState');
    return fy_state_ext.valueOf(state!);
  }

  Future<String?> get platformVersion async {
    final String? version = await _channel.invokeMethod('getPlatformVersion');
    return version;
  }

  Future<String> get deviceId async {
    final String deviceId = await _channel.invokeMethod('getDeviceId');
    return deviceId;
  }

  Future<bool> prepare() async {
    var preapred = await _channel.invokeMethod('prepare');

    if (preapred == true || preapred == 1) {
      return true;
    }

    return false;
  }

  Future<bool> prepared() async {
    var preapred = await _channel.invokeMethod('prepared');

    if (preapred == true || preapred == 1) {
      return true;
    }

    return false;
  }

  Future<void> start(parametersMap) async {
    await _channel.invokeMethod('start', parametersMap);
  }

  Future<void> stop() async {
    await _channel.invokeMethod('stop');
  }

  Future<void> startVpnService(String protocol, String serverIp, int serverPort,
      String username, String password, String? cert) async {
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
