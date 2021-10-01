package com.oceanbase.clogproxy.util;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.core.type.TypeReference;
import com.fasterxml.jackson.databind.ObjectMapper;

import java.io.File;
import java.io.IOException;
import java.util.Map;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-06-12
 * <p>
 * JSON API wrapper in case of json dependency could be replaced
 */
@SuppressWarnings("unchecked")
public class JsonHint {

    private static ObjectMapper json = new ObjectMapper();

    public static <T> T load(String filename, Class<T> clazz) {
        try {
            File file = new File(filename);
            return json.readValue(file, clazz);
        } catch (IOException e) {
            return null;
        }
    }

    public static Map<String, Object> load(String filename) throws Exception {
        File file = new File(filename);
        return json.readValue(file, new TypeReference<Map<String, Object>>() {});
    }

    public static <T> T json2obj(String jsonStr, Class<T> clazz) {
        if (String.class == clazz) { // string just return
            return (T) jsonStr;
        }

        try {
            return json.readValue(jsonStr, clazz);
        } catch (IOException e) {
            return null;
        }
    }

    public static String obj2json(Object object) {
        try {
            return json.writeValueAsString(object);
        } catch (JsonProcessingException e) {
            return "";
        }
    }
}
