package com.oceanbase.clogproxy.capture;

import com.oceanbase.clogproxy.common.packet.LogType;

/**
 * <p>
 * </p>
 * <p> Date: 2020-04-16 Time: 16:28</p>
 *
 * @author jiyong.jy
 */
public class SourceInvokeManager {

    public SourceInvokeManager instance() {
        return Singleton.INSTANCE;
    }

    private static class Singleton {
        private static final SourceInvokeManager INSTANCE = new SourceInvokeManager();
    }

    private SourceInvokeManager() {}

    private static final SourceInvoke DRC_STORE_PROCESSOR = new DrcStoreInvoke();
    private static final SourceInvoke LOGREADER_PROCESSOR = new LogReaderInvoke();

    public static boolean init() {
        if (!DRC_STORE_PROCESSOR.init()) {
            return false;
        }
        return LOGREADER_PROCESSOR.init();
    }

    public static SourceInvoke getInvoke(LogType logReaderType) {
        switch (logReaderType) {
            case OMS_STORE:
                return DRC_STORE_PROCESSOR;
            case OCEANBASE:
                return LOGREADER_PROCESSOR;
            default:
                throw new RuntimeException("Unsupport LogReaderType " + logReaderType);
        }
    }
}
