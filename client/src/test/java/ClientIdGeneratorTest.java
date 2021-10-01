import com.oceanbase.clogproxy.client.util.ClientIdGenerator;
import org.junit.Test;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * @author Fankux(yuqi.fy)
 * @since 2020-07-09
 */
public class ClientIdGeneratorTest {
    private static final Logger logger = LoggerFactory.getLogger(ClientIdGeneratorTest.class);

    @Test
    public void testClientIdGenerator() {
        logger.info("ClientId: {}", ClientIdGenerator.generate());
    }
}
