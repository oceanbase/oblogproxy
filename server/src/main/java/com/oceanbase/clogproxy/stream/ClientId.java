package com.oceanbase.clogproxy.stream;

import java.util.Objects;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-07-07
 * <p>
 * Never befuddled with String
 */
public class ClientId {
    private String id;

    public static ClientId EMPTY = new ClientId();

    public static ClientId of(String id) {
        ClientId clientId = new ClientId();
        clientId.set(id);
        return clientId;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (o == null || getClass() != o.getClass()) {
            return false;
        }
        ClientId clientId = (ClientId) o;
        return Objects.equals(id, clientId.id);
    }

    @Override
    public int hashCode() {
        return Objects.hash(id);
    }

    @Override
    public String toString() {
        return id;
    }

    public String get() {
        return id;
    }

    public void set(String id) {
        this.id = id;
    }
}
