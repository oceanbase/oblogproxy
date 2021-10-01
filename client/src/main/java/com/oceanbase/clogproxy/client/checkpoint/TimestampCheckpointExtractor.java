package com.oceanbase.clogproxy.client.checkpoint;

import com.oceanbase.oms.record.oms.OmsRecord;

/**
 * <p>
 * </p>
 * <p> Date: 2020-05-07 Time: 11:30</p>
 *
 * @author jiyong.jy
 */
public class TimestampCheckpointExtractor implements CheckpointExtractor {

    @Override
    public String extract(OmsRecord record) {
        return record.meta().getCheckpoint();
    }

}
