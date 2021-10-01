package com.oceanbase.clogproxy.util;

import org.apache.commons.io.Charsets;
import org.apache.http.client.HttpClient;
import org.apache.http.HttpResponse;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.impl.client.HttpClients;
import org.apache.http.util.EntityUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-07-14
 */
public class HttpAccess {
    private static final Logger logger = LoggerFactory.getLogger(HttpAccess.class);

    public static <T> T get(String url, Class<T> clazz) {
        HttpClient http = HttpClients.createDefault();
        HttpGet get = new HttpGet(url);

        try {
            HttpResponse response = http.execute(get);
            int responseCode = response.getStatusLine().getStatusCode();
            if (responseCode != 200) {
                logger.error("failed to GET http: {}, error return: {}, {}", url, responseCode, response.getStatusLine().getReasonPhrase());
                return null;
            }

            String payloadBody = EntityUtils.toString(response.getEntity(), Charsets.UTF_8);
            return JsonHint.json2obj(payloadBody, clazz);

        } catch (IOException e) {
            logger.error("failed to GET http: {}: ", url, e);
            return null;
        }
    }
}
