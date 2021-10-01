package com.oceanbase.logclient;

import java.util.HashMap;

import com.oceanbase.oms.store.client.message.drcmessage.DrcNETBinaryRecord;

public class LibOblogReaderJni {
    static {
        System.loadLibrary("oblogreader_jni");
    }

    public static native boolean init(HashMap<String, String> configs, long start_timestamp);

    public static native boolean start();

    public static native void stop();

    public static native DrcNETBinaryRecord next();

    public static native DrcNETBinaryRecord next(long timeout_us);
}