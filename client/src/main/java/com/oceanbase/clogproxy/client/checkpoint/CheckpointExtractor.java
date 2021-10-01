package com.oceanbase.clogproxy.client.checkpoint;

import com.oceanbase.oms.record.oms.OmsRecord;

/**
 * <p>
 * </p>
 * <p> Date: 2020-05-07 Time: 11:29</p>
 *
 * @author jiyong.jy
 */
public interface CheckpointExtractor {

    String extract(OmsRecord record);

}
