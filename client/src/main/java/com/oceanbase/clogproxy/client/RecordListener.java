package com.oceanbase.clogproxy.client;

import com.oceanbase.oms.record.oms.OmsRecord;

/**
 * <p>
 * </p>
 * <p> Date: 2020-05-01 Time: 11:13</p>
 *
 * @author jiyong.jy
 */
public interface RecordListener {

    void notify(OmsRecord record);

    void onException(LogProxyClientException e);
}
