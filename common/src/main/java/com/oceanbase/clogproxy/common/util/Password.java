package com.oceanbase.clogproxy.common.util;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-07-22
 */
public class Password {
    private String value;

    public void set(String value) {
        this.value = value;
    }

    public String get() {
        return value;
    }

    @Override
    public String toString() {
        return "******";
    }
}
