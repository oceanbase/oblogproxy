package com.oceanbase.clogproxy;

import com.oceanbase.clogproxy.common.util.TypeTrait;
import org.junit.Assert;
import org.junit.Test;

import java.lang.reflect.Field;
import java.util.Arrays;
import java.util.List;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-06-13
 */
public class TypeTraitTest {

    static class Types {
        static byte b;
        static Byte bp = 1;
        static short s;
        static Short sp = 1;
        static int i;
        static Integer ip = 1;
        static long l;
        static Long L = 1L;
        static float f;
        static Float fp = 1.1f;
        static double d;
        static Double dp = 2.3333d;
        static boolean bool;
        static Boolean boolp = true;
        static char c;
        static Character cp = 'c';
        static String str = "abcde";
    }

    @Test
    public void testType() {
        Assert.assertTrue(TypeTrait.isNumber(Types.b));
        Assert.assertTrue(TypeTrait.isNumber(Types.bp));
        Assert.assertTrue(TypeTrait.isNumber(Types.s));
        Assert.assertTrue(TypeTrait.isNumber(Types.sp));
        Assert.assertTrue(TypeTrait.isNumber(Types.i));
        Assert.assertTrue(TypeTrait.isNumber(Types.ip));
        Assert.assertTrue(TypeTrait.isNumber(Types.l));
        Assert.assertTrue(TypeTrait.isNumber(Types.L));
        Assert.assertTrue(TypeTrait.isReal(Types.f));
        Assert.assertTrue(TypeTrait.isReal(Types.fp));
        Assert.assertTrue(TypeTrait.isReal(Types.d));
        Assert.assertTrue(TypeTrait.isReal(Types.dp));
        Assert.assertTrue(TypeTrait.isBool(Types.bool));
        Assert.assertTrue(TypeTrait.isBool(Types.boolp));
        Assert.assertTrue(TypeTrait.isString(Types.c));
        Assert.assertTrue(TypeTrait.isString(Types.cp));
        Assert.assertTrue(TypeTrait.isString(Types.str));
    }

    @Test
    public void testReflectType() throws Exception {
        Field[] reflectFields = Types.class.getDeclaredFields();
        Assert.assertEquals(17, reflectFields.length);
        Assert.assertTrue(TypeTrait.isNumber(reflectFields[0]));
        Assert.assertTrue(TypeTrait.isNumber(reflectFields[1]));
        Assert.assertTrue(TypeTrait.isNumber(reflectFields[2]));
        Assert.assertTrue(TypeTrait.isNumber(reflectFields[3]));
        Assert.assertTrue(TypeTrait.isNumber(reflectFields[4]));
        Assert.assertTrue(TypeTrait.isNumber(reflectFields[5]));
        Assert.assertTrue(TypeTrait.isNumber(reflectFields[6]));
        Assert.assertTrue(TypeTrait.isNumber(reflectFields[7]));
        Assert.assertTrue(TypeTrait.isReal(reflectFields[8]));
        Assert.assertTrue(TypeTrait.isReal(reflectFields[9]));
        Assert.assertTrue(TypeTrait.isReal(reflectFields[10]));
        Assert.assertTrue(TypeTrait.isReal(reflectFields[11]));
        Assert.assertTrue(TypeTrait.isBool(reflectFields[12]));
        Assert.assertTrue(TypeTrait.isBool(reflectFields[13]));
        Assert.assertTrue(TypeTrait.isString(reflectFields[14]));
        Assert.assertTrue(TypeTrait.isString(reflectFields[15]));
        Assert.assertTrue(TypeTrait.isString(reflectFields[16]));
    }

    @Test
    public void testSameLooseType() {
        Field[] reflectFields = Types.class.getDeclaredFields();
        Assert.assertEquals(17, reflectFields.length);

        List<Object> numbers = Arrays.asList(Types.b, Types.bp, Types.s, Types.sp, Types.i, Types.ip, Types.l, Types.L);
        List<Object> reals = Arrays.asList(Types.f, Types.fp, Types.d, Types.dp);
        List<Object> bools = Arrays.asList(Types.bool, Types.boolp);
        List<Object> strs = Arrays.asList(Types.c, Types.cp, Types.str);
        // number
        for (Object number : numbers) {
            for (int i = 0; i < 8; ++i) {
                Assert.assertTrue(TypeTrait.isSameLooseType(number, reflectFields[i]));
            }
            for (int i = 0; i < 17; ++i) {
                if (i < 8) {
                    continue;
                }
                Assert.assertFalse(TypeTrait.isSameLooseType(number, reflectFields[i]));
            }
        }
        // real
        for (Object real : reals) {
            for (int i = 0; i < 4; ++i) {
                Assert.assertTrue(TypeTrait.isSameLooseType(real, reflectFields[i + 8]));
            }
            for (int i = 0; i < 17; ++i) {
                if (i >= 8 && i < 12) {
                    continue;
                }
                Assert.assertFalse(TypeTrait.isSameLooseType(real, reflectFields[i]));
            }
        }
        // bool
        for (Object bool : bools) {
            for (int i = 0; i < 2; ++i) {
                Assert.assertTrue(TypeTrait.isSameLooseType(bool, reflectFields[i + 12]));
            }
            for (int i = 0; i < 17; ++i) {
                if (i >= 12 && i < 14) {
                    continue;
                }
                Assert.assertFalse(TypeTrait.isSameLooseType(bool, reflectFields[i]));
            }
        }
        // string
        for (Object str : strs) {
            for (int i = 0; i < 3; ++i) {
                Assert.assertTrue(TypeTrait.isSameLooseType(str, reflectFields[i + 14]));
            }
            for (int i = 0; i < 17; ++i) {
                if (i >= 14) {
                    continue;
                }
                Assert.assertFalse(TypeTrait.isSameLooseType(str, reflectFields[i]));
            }
        }
    }

    @Test
    public void testFromString() {
        Byte B = TypeTrait.fromString("1", Byte.class);
        Assert.assertEquals(B, Types.bp);

        Short S = TypeTrait.fromString("1", Short.class);
        Assert.assertEquals(S, Types.sp);

        Integer I = TypeTrait.fromString("1", Integer.class);
        Assert.assertEquals(I, Types.ip);

        Long L = TypeTrait.fromString("1", Long.class);
        Assert.assertEquals(L, Types.L);

        Float F = TypeTrait.fromString("1.1", Float.class);
        Assert.assertEquals(F, Types.fp);

        Double D = TypeTrait.fromString("2.3333", Double.class);
        Assert.assertEquals(D, Types.dp);

        Boolean Bool = TypeTrait.fromString("true", Boolean.class);
        Assert.assertEquals(Bool, true);
        Bool = TypeTrait.fromString("false", Boolean.class);
        Assert.assertEquals(Bool, false);

        Character C = TypeTrait.fromString("ccc", Character.class);
        Assert.assertEquals(C, Types.cp);

        String str = TypeTrait.fromString("abcde", String.class);
        Assert.assertEquals(str, Types.str);

    }
}
