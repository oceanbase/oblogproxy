package com.oceanbase.clogproxy.common.util;

import org.apache.commons.lang3.StringUtils;

import java.lang.reflect.Field;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-06-13
 */
public class TypeTrait {
    public static boolean isNumber(Object obj) {
        return (obj instanceof Byte) || (obj instanceof Short) ||
                (obj instanceof Integer) || (obj instanceof Long);
    }

    public static boolean isNumber(Field field) {
        String typeName = field.getGenericType().getTypeName();
        return typeName.equals("byte") || typeName.equals("java.lang.Byte") ||
                typeName.equals("short") || typeName.equals("java.lang.Short") ||
                typeName.equals("int") || typeName.equals("java.lang.Integer") ||
                typeName.equals("long") || typeName.equals("java.lang.Long");
    }

    public static boolean isReal(Object obj) {
        return (obj instanceof Float) || (obj instanceof Double);
    }

    public static boolean isReal(Field field) {
        String typeName = field.getGenericType().getTypeName();
        return typeName.equals("float") || typeName.equals("java.lang.Float") ||
                typeName.equals("double") || typeName.equals("java.lang.Double");
    }

    public static boolean isBool(Object obj) {
        return obj instanceof Boolean;
    }

    public static boolean isBool(Field field) {
        String typeName = field.getGenericType().getTypeName();
        return typeName.equals("boolean") || typeName.equals("java.lang.Boolean");
    }

    public static boolean isString(Object obj) {
        return (obj instanceof Character) || (obj instanceof String);
    }

    public static boolean isString(Field field) {
        String typeName = field.getGenericType().getTypeName();
        return typeName.equals("char") || typeName.equals("java.lang.Character") ||
                typeName.equals("java.lang.String");
    }

    public static boolean isSameLooseType(Object object, Field field) {
        return (isNumber(object) && isNumber(field)) ||
                (isReal(object) && isReal(field)) ||
                (isBool(object) && isBool(field)) ||
                (isString(object) && isString(field));
    }

    @SuppressWarnings("unchecked")
    public static <T> T fromString(String value, Class<?> clazz) {
        if (clazz == Byte.class || clazz == byte.class) {
            return (T) Byte.valueOf(value);
        } else if (clazz == Short.class || clazz == short.class) {
            return (T) Short.valueOf(value);
        } else if (clazz == Integer.class || clazz == int.class) {
            return (T) Integer.valueOf(value);
        } else if (clazz == Long.class || clazz == long.class) {
            return (T) Long.valueOf(value);
        } else if (clazz == Float.class || clazz == float.class) {
            return (T) Float.valueOf(value);
        } else if (clazz == Double.class || clazz == double.class) {
            return (T) Double.valueOf(value);
        } else if (clazz == Boolean.class || clazz == boolean.class) {
            return (T) (Boolean) (!StringUtils.isEmpty(value) && Boolean.parseBoolean(value));
        } else if (clazz == Character.class || clazz == char.class) {
            if (StringUtils.isNotEmpty(value)) {
                return (T) (Character) value.charAt(0);
            }
        } else if (clazz == String.class) {
            return (T) value;
        }
        return null;
    }
}
