package xyz.coolblog.httpd.json.model;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import xyz.coolblog.httpd.json.BeautifyJsonUtils;
import xyz.coolblog.httpd.json.exception.JsonTypeException;

/**
 * Created by code4wt on 17/5/19.
 */
public class JsonArray implements Iterable {

    private List list = new ArrayList();

    public void add(Object obj) {
        list.add(obj);
    }

    public Object get(int index) {
        return list.get(index);
    }

    public int size() {
        return list.size();
    }

    public JsonObject getJsonObject(int index) {
        Object obj = list.get(index);
        if (!(obj instanceof JsonObject)) {
            throw new JsonTypeException("Type of value is not JsonObject");
        }

        return (JsonObject) obj;
    }

    public JsonArray getJsonArray(int index) {
        Object obj = list.get(index);
        if (!(obj instanceof JsonArray)) {
            throw new JsonTypeException("Type of value is not JsonArray");
        }

        return (JsonArray) obj;
    }

    @Override
    public String toString() {
        return BeautifyJsonUtils.beautify(this);
    }

    @Override
    public Iterator iterator() {
        return list.iterator();
    }
}
