package com.oceanbase.clogproxy.capture;

import java.util.List;

/**
 * <p>
 * </p>
 * <p> Date: 2020-04-16 Time: 16:27</p>
 *
 * @author jiyong.jy
 */
public interface SourceInvoke {

    boolean init();

    boolean start(String clientId, String configuration);

    boolean isAlive(SourceProcess source);

    void stop(SourceProcess source);

    void clean(SourcePath path);

    List<SourceProcess> listProcesses();

    List<SourcePath> listPaths();

}
