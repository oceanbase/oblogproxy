package com.oceanbase.clogproxy;

import com.oceanbase.clogproxy.common.util.TaskExecutor;
import io.netty.buffer.ByteBuf;
import io.netty.buffer.ByteBufAllocator;
import io.netty.buffer.CompositeByteBuf;
import io.netty.util.ReferenceCountUtil;
import org.junit.Test;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.concurrent.TimeUnit;

import static org.junit.Assert.*;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-06-14
 */
public class NettySliceTest {
    private static Logger logger = LoggerFactory.getLogger(NettySliceTest.class);

    private static class SliceArg {
        private ByteBuf slice;
        private boolean sleepFirst = false;

        SliceArg(ByteBuf slice) {
            this(slice, false);
        }

        SliceArg(ByteBuf slice, boolean sleepFirst) {
            this.slice = slice;
            this.sleepFirst = sleepFirst;
        }

        public void run() {
            if (sleepFirst) {
                try {
                    TimeUnit.SECONDS.sleep(1);
                } catch (InterruptedException e) {
                    // do nothing
                }
            }
            logger.info("slice: {}", slice.toString());
            if (slice.refCnt() == 0) {
                logger.info("slice released, ignore");
            } else {
                slice.release();
            }
            assertEquals(slice.refCnt(), 0);
        }
    }

    @Test
    public void testSlice() throws Exception {
        ByteBuf buf = ByteBufAllocator.DEFAULT.buffer(100);
        buf.writeInt(111);
        assertEquals(buf.refCnt(), 1);

        ByteBuf slice = buf.retainedSlice();
        assertEquals(buf.refCnt(), 2);
        assertEquals(slice.refCnt(), 1);

        // use slice to release
        slice.release();
        assertEquals(buf.refCnt(), 1);
        assertEquals(slice.refCnt(), 0);

        final ByteBuf slice2 = buf.retainedSlice();
        assertEquals(buf.refCnt(), 2);
        assertEquals(slice2.refCnt(), 1);

        // use slice to release in other thread
        TaskExecutor.Task<Void> task = TaskExecutor.instance().async(() -> {
            new SliceArg(slice2).run();
            return null;
        });

        TimeUnit.SECONDS.sleep(1);
        assertEquals(buf.refCnt(), 1);
        assertEquals(slice2.refCnt(), 0);

        logger.info("buf: {}", buf.toString());
        buf.release();
        assertEquals(buf.refCnt(), 0);

        task.get();

        // relese buf first, then use slice to release
        buf = ByteBufAllocator.DEFAULT.buffer(100);
        buf.writeInt(111);
        assertEquals(buf.refCnt(), 1);

        slice = buf.retainedSlice();
        assertEquals(buf.refCnt(), 2);
        assertEquals(slice.refCnt(), 1);

        buf.release();
        assertEquals(buf.refCnt(), 1);
        assertEquals(slice.refCnt(), 1);

        slice.release();
        assertEquals(buf.refCnt(), 0);
        assertEquals(slice.refCnt(), 0);

        // relese buf first, then use slice to rel
        buf = ByteBufAllocator.DEFAULT.buffer(100);
        buf.writeInt(111);
        assertEquals(buf.refCnt(), 1);

        final ByteBuf sliceAsync = buf.retainedSlice();
        assertEquals(buf.refCnt(), 2);
        assertEquals(sliceAsync.refCnt(), 1);

        buf.release();
        assertEquals(buf.refCnt(), 1);
        assertEquals(sliceAsync.refCnt(), 1);

        task = TaskExecutor.instance().async(() -> {
            new SliceArg(sliceAsync, true).run();
            return null;
        });

        ReferenceCountUtil.release(buf);
        assertEquals(buf.refCnt(), 0);

        task.get();
        assertEquals(sliceAsync.refCnt(), 0);
    }

    private static class CBufArg {
        private CompositeByteBuf cbuf;
        private boolean sleepFirst = false;

        CBufArg(CompositeByteBuf cbuf, boolean sleepFirst) {
            this.cbuf = cbuf;
            this.sleepFirst = sleepFirst;
        }

        public void run() {
            if (sleepFirst) {
                try {
                    TimeUnit.SECONDS.sleep(1);
                } catch (InterruptedException e) {
                    // do nothing
                }
            }

            logger.info("CompositeByteBuf: {}", cbuf.toString());
            cbuf.release();
            assertEquals(cbuf.refCnt(), 0);
        }
    }

    @Test
    public void testCompositeBufRemoteReleaseFirst() throws Exception {
        ByteBuf buf = ByteBufAllocator.DEFAULT.buffer(100);
        buf.writeInt(111);
        assertEquals(buf.refCnt(), 1);

        ByteBuf slice = buf.retainedSlice();
        assertEquals(buf.refCnt(), 2);
        assertEquals(slice.refCnt(), 1);

        CompositeByteBuf cbuf = ByteBufAllocator.DEFAULT.compositeBuffer();
        cbuf.addComponent(true, slice);
        assertEquals(cbuf.refCnt(), 1);

        cbuf.retain();  // reference count detach
        assertEquals(buf.refCnt(), 2);
        assertEquals(slice.refCnt(), 1);
        assertEquals(cbuf.refCnt(), 2);

        cbuf.release();
        assertEquals(buf.refCnt(), 2);
        assertEquals(slice.refCnt(), 1);
        assertEquals(cbuf.refCnt(), 1);

        TaskExecutor.Task<Void> task = TaskExecutor.instance().async(() -> {
            new CBufArg(cbuf, false).run();
            return null;
        });

        TimeUnit.SECONDS.sleep(1);
        assertEquals(buf.refCnt(), 1);
        assertEquals(slice.refCnt(), 0);

        logger.info("buf: {}", buf.toString());
        buf.release();
        assertEquals(buf.refCnt(), 0);

        task.join();
    }

    @Test
    public void testCompositeBufHereReleaseFirst() throws Exception {
        ByteBuf buf = ByteBufAllocator.DEFAULT.buffer(100);
        buf.writeInt(111);
        assertEquals(buf.refCnt(), 1);

        ByteBuf slice = buf.retainedSlice();
        assertEquals(buf.refCnt(), 2);
        assertEquals(slice.refCnt(), 1);

        CompositeByteBuf cbuf = ByteBufAllocator.DEFAULT.compositeBuffer();
        cbuf.addComponent(true, slice);
        assertEquals(cbuf.refCnt(), 1);

        cbuf.retain();  // reference count detach
        assertEquals(buf.refCnt(), 2);
        assertEquals(slice.refCnt(), 1);
        assertEquals(cbuf.refCnt(), 2);

        cbuf.release();
        assertEquals(buf.refCnt(), 2);
        assertEquals(slice.refCnt(), 1);
        assertEquals(cbuf.refCnt(), 1);

        TaskExecutor.Task<Void> task = TaskExecutor.instance().async(() -> {
            new CBufArg(cbuf, true).run();
            return null;
        });
        logger.info("buf: {}", buf.toString());
        buf.release();
        assertEquals(buf.refCnt(), 1);

        TimeUnit.SECONDS.sleep(2);
        assertEquals(buf.refCnt(), 0);
        assertEquals(slice.refCnt(), 0);
        assertEquals(cbuf.refCnt(), 0);

        task.join();
    }

}
