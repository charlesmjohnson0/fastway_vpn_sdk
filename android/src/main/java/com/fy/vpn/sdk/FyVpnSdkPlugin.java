package com.fy.vpn.sdk;

import static android.app.Activity.RESULT_OK;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;


import androidx.annotation.NonNull;

import com.fy.vpn.core.FyVpnService;
import com.fy.vpn.core.StateEnum;

import java.util.Map;
import java.util.UUID;

import io.flutter.embedding.engine.plugins.FlutterPlugin;
import io.flutter.embedding.engine.plugins.activity.ActivityAware;
import io.flutter.embedding.engine.plugins.activity.ActivityPluginBinding;
import io.flutter.plugin.common.EventChannel;
import io.flutter.plugin.common.MethodCall;
import io.flutter.plugin.common.MethodChannel;
import io.flutter.plugin.common.MethodChannel.MethodCallHandler;
import io.flutter.plugin.common.MethodChannel.Result;
import io.flutter.plugin.common.PluginRegistry;

/**
 * FyVpnSdkPlugin
 */
public class FyVpnSdkPlugin implements FlutterPlugin, MethodCallHandler, EventChannel.StreamHandler, ActivityAware {
    /// The MethodChannel that will the communication between Flutter and native Android
    ///
    /// This local reference serves to register the plugin with the Flutter Engine and unregister it
    /// when the Flutter Engine is detached from the Activity
    private ActivityPluginBinding activityPluginBinding;
    private EventChannel.EventSink eventSink;
    private MethodChannel methodChannel;
    private EventChannel eventChannel;

    @Override
    public void onAttachedToEngine(@NonNull FlutterPluginBinding flutterPluginBinding) {
        methodChannel = new MethodChannel(flutterPluginBinding.getBinaryMessenger(), "fy_vpn");
        eventChannel = new EventChannel(flutterPluginBinding.getBinaryMessenger(), "fy_vpn_states");
        methodChannel.setMethodCallHandler(this);
        eventChannel.setStreamHandler(this);
    }

    @Override
    public void onMethodCall(@NonNull MethodCall call, @NonNull Result result) {

        switch (call.method) {
            case "getPlatformVersion":
                result.success("Android " + android.os.Build.VERSION.RELEASE);
                break;
            case "getDeviceId":
//                String android_id = Secure.getString(getContext().getContentResolver(),
//                        Secure.ANDROID_ID);
                result.success(UUID.randomUUID().toString());
                break;
            case "prepare":
                Intent prepare = FyVpnService.prepare(activityPluginBinding.getActivity().getApplicationContext());
                if (prepare != null) {
                    PluginRegistry.ActivityResultListener listener = new PluginRegistry.ActivityResultListener() {
                        @Override
                        public boolean onActivityResult(int requestCode, int resultCode, Intent data) {

                            if (requestCode == 100 && resultCode == RESULT_OK) {
                                result.success(true);
                            } else {
                                result.success(false);
                            }
                            activityPluginBinding.removeActivityResultListener(this);
                            return true;
                        }
                    };
                    activityPluginBinding.addActivityResultListener(listener);
                    activityPluginBinding.getActivity().startActivityForResult(prepare, 100);
                } else {
                    result.success(true);
                }
                break;
            case "prepared":
                Intent prepared = FyVpnService.prepare(activityPluginBinding.getActivity().getApplicationContext());
                result.success(prepared == null);
                break;
            case "getState":
                result.success(this.state.ordinal());
                break;
            case "getError":
                result.success(error);
                break;
            case "start":
                Map<String, String> parameters = (Map<String, String>) call.arguments;
                FyVpnService.startVpnService(this.activityPluginBinding.getActivity(), parameters);
                result.success(true);
                break;
            case "stop":
                FyVpnService.stopVpnService(this.activityPluginBinding.getActivity());
                result.success(true);
                break;
            default:
                result.notImplemented();
        }
    }

    private BroadcastReceiver receiver = null;
    private StateEnum state = StateEnum.NONE;
    private int error = 0;


    protected void registerReceiver() {

        unregisterReceiver();

        this.receiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {

                if (eventSink != null) {

                    int stateInt = intent.getIntExtra("state", 0);

                    if (stateInt > 0) {
                        state = StateEnum.valueOf(stateInt);
                        eventSink.success(state.ordinal());
                    }

                    int errorInt = intent.getIntExtra("error", 0);

                    if (errorInt < 0) {
                        error = errorInt;
                        eventSink.success(error);
                    }
                }
            }
        };

        IntentFilter filter = new IntentFilter();

        filter.addAction(FyVpnService.FY_VPN_SERVICE_BROADCAST_ACTION);

        activityPluginBinding.getActivity().registerReceiver(receiver, filter);
    }

    protected void unregisterReceiver() {
        if (receiver != null) {
            activityPluginBinding.getActivity().unregisterReceiver(receiver);
            receiver = null;
        }
    }

    @Override
    public void onDetachedFromEngine(@NonNull FlutterPluginBinding binding) {
        methodChannel.setMethodCallHandler(null);
        eventChannel.setStreamHandler(null);
    }

    @Override
    public void onListen(Object arguments, EventChannel.EventSink events) {
        this.eventSink = events;
    }

    @Override
    public void onCancel(Object arguments) {
        this.eventSink = null;
    }

    @Override
    public void onAttachedToActivity(@NonNull ActivityPluginBinding binding) {
        this.activityPluginBinding = binding;
        registerReceiver();
    }

    @Override
    public void onDetachedFromActivityForConfigChanges() {
        unregisterReceiver();
    }

    @Override
    public void onReattachedToActivityForConfigChanges(@NonNull ActivityPluginBinding binding) {
        this.activityPluginBinding = binding;
        registerReceiver();
    }

    @Override
    public void onDetachedFromActivity() {
        unregisterReceiver();
    }
}
