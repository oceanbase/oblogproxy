package com.oceanbase.clogproxy;

import com.oceanbase.clogproxy.capture.SourceInvokeManager;
import com.oceanbase.clogproxy.capture.SourceProcess;
import com.oceanbase.clogproxy.common.packet.LogType;
import com.oceanbase.clogproxy.metric.LogProxyMetric;
import com.oceanbase.clogproxy.metric.Monitor;
import com.oceanbase.clogproxy.obaccess.OBAccess;
import com.oceanbase.clogproxy.server.LogReaderServer;
import com.oceanbase.clogproxy.server.LogProxyServer;
import com.oceanbase.clogproxy.stream.StreamManager;
import com.oceanbase.clogproxy.util.Conf;
import com.oceanbase.clogproxy.common.util.CryptoUtil;
import com.oceanbase.clogproxy.common.util.Hex;
import com.oceanbase.clogproxy.common.util.TaskExecutor;
import org.apache.commons.cli.CommandLine;
import org.apache.commons.cli.CommandLineParser;
import org.apache.commons.cli.DefaultParser;
import org.apache.commons.cli.Option;
import org.apache.commons.cli.Options;
import org.apache.commons.cli.ParseException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * <p>
 * </p>
 * <p> Date: 2020-04-20 Time: 21:40</p>
 *
 * @author jiyong.jy
 */
public class Bootstrap {
    private static Logger logger = LoggerFactory.getLogger(Bootstrap.class);

    public static void main(String[] args) {

        Options options = new Options();
        options.addOption("h", "help", false, "help");
        options.addOption("f", "file", true, "configuration json file");
        options.addOption("P", "listenPort", true, "listen port");
        options.addOption("C", "capturerPort", true, "capturer listen port");
        options.addOption("D", "debug", false, "debug mode not send message");
        options.addOption("V", "verbose", false, "print plentiful log");
        options.addOption("x", "encrypt", true, "encrypt text");
        options.addOption("y", "decrypt", true, "decrypt text");

        Map<String, String> inopts = new HashMap<>();
        String file = "./conf/conf.json";
        CommandLineParser parser = new DefaultParser();
        try {
            CommandLine cmd = parser.parse(options, args);
            for (Option opt : cmd.getOptions()) {
                if ("f".equals(opt.getOpt())) {
                    file = opt.getValue();
                } else {
                    inopts.put(opt.getOpt(), opt.getValue());
                }
            }
        } catch (ParseException e) {
            logger.info("");
            return;
        }

        if (!Conf.load(file)) {
            System.exit(-1);
        }

        // command line options overwrite configuration options
        for (Map.Entry<String, String> entry : inopts.entrySet()) {
            switch (entry.getKey()) {
                case "P":
                    Conf.PROXY_SERVICE_PORT = Integer.parseInt(entry.getValue());
                    break;
                case "C":
                    Conf.CAPTURE_SERVICE_PORT = Integer.parseInt(entry.getValue());
                    break;
                case "D":
                    Conf.DEBUG = true;
                    logger.info("enable DEBUG mode");
                    break;
                case "V":
                    Conf.VERBOSE = true;
                    logger.info("enable VERBOSE log");
                    break;
                case "x":
                    System.out.println(Hex.str(CryptoUtil.newEncryptor().encrypt(entry.getValue())));
                    return;
                case "y":
                    System.out.println(CryptoUtil.newEncryptor().decrypt(Hex.toBytes(entry.getValue())));
                    return;
                case "h":
                default:
                    for (Option option : options.getOptions()) {
                        System.out.printf("-%s,--%s\t\t\t%s%n", option.getOpt(), option.getLongOpt(), option.getDescription());
                    }
                    return;
            }
        }

        if (!SourceInvokeManager.init()) {
            System.exit(-1);
        }

        if (!OBAccess.instance().init()) {
            System.exit(-1);
        }

        if (!StreamManager.instance().init()) {
            System.exit(-1);
        }

        if (!Monitor.instance().init()) {
            System.exit(-1);
        }

        if (!LogProxyMetric.instance().init()) {
            System.exit(-1);
        }

        Bootstrap bootstrap = new Bootstrap();
        bootstrap.start();
    }

    private LogProxyServer logProxyServer;
    private LogReaderServer logReaderServer;

    public Bootstrap() {
        this.logProxyServer = LogProxyServer.instance();
        this.logReaderServer = LogReaderServer.instance();
    }

    public void start() {
        startLogProxyServer();
        startLogCaptureServer();
    }

    public void stop() {
        // Kill all LogReader
        List<SourceProcess> logreaders = SourceInvokeManager.getInvoke(LogType.OCEANBASE).listProcesses();
        for (SourceProcess logreader : logreaders) {
            SourceInvokeManager.getInvoke(LogType.OCEANBASE).stop(logreader);
        }
    }

    private void startLogProxyServer() {
        TaskExecutor.instance().background(() -> {
            try {
                logProxyServer.start();
            } catch (Exception e) {
                logger.error("failed to start logProxyServer: ", e);
                System.exit(-1);
            }
            return null;
        });
    }

    private void startLogCaptureServer() {
        TaskExecutor.instance().background(() -> {
            try {
                logReaderServer.start();
            } catch (Exception e) {
                logger.error("failed to start logReaderServer: ", e);
                System.exit(-1);
            }
            return null;
        });
    }
}
