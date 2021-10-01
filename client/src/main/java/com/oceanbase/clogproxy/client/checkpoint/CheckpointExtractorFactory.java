package com.oceanbase.clogproxy.client.checkpoint;

import com.oceanbase.clogproxy.common.packet.LogType;

/**
 * <p>
 * </p>
 * <p> Date: 2020-05-07 Time: 11:34</p>
 *
 * @author jiyong.jy
 */
public class CheckpointExtractorFactory {

    private static final TimestampCheckpointExtractor TIMESTAMP_CHECKPOINT_EXTRACTOR = new TimestampCheckpointExtractor();

    public static CheckpointExtractor getCheckpointExtractor(LogType logType) {
        switch (logType) {
            case OMS_STORE:
            case OCEANBASE:
                return TIMESTAMP_CHECKPOINT_EXTRACTOR;
            default:
                throw new IllegalArgumentException("unknown log type " + logType);
        }
    }
}
