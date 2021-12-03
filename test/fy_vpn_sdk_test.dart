import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:fy_vpn_sdk/fy_vpn_sdk.dart';

void main() {
  const MethodChannel channel = MethodChannel('fy_vpn_sdk');

  TestWidgetsFlutterBinding.ensureInitialized();

  setUp(() {
    channel.setMockMethodCallHandler((MethodCall methodCall) async {
      return '42';
    });
  });

  tearDown(() {
    channel.setMockMethodCallHandler(null);
  });

  test('getPlatformVersion', () async {
    expect(await FyVpnSdk.platformVersion, '42');
  });
}
