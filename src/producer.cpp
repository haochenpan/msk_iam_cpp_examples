#include <aws/core/Aws.h>
#include "AwsMskIamSigner.h"
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <librdkafka/rdkafka.h>

static void
dr_msg_cb(rd_kafka_t *rk, const rd_kafka_message_t *rkmessage, void *opaque)
{
    if (rkmessage->err)
        fprintf(stderr, "%% Message delivery failed: %s\n",
                rd_kafka_err2str(rkmessage->err));
    else
        fprintf(stderr,
                "%% Message delivered (%zd bytes, "
                "partition %" PRId32 ")\n",
                rkmessage->len, rkmessage->partition);

    /* The rkmessage is destroyed automatically by librdkafka */
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <topic> <message>" << std::endl;
        return 1;
    }

    const char* required_env_vars[] = {
        "AWS_ACCESS_KEY_ID",
        "AWS_SECRET_ACCESS_KEY",
        "AWS_REGION",
        "OCTOPUS_BOOTSTRAP_SERVERS"
    };
    for (const char* var : required_env_vars) {
        if (!std::getenv(var)) {
            std::cerr << "Missing required environment variable: " << var << std::endl;
            return 1;
        }
    }
    
    const char *brokers = std::getenv("OCTOPUS_BOOTSTRAP_SERVERS");
    if (!brokers)
    {
        std::cerr << "OCTOPUS_BOOTSTRAP_SERVERS environment variable not set." << std::endl;
        return 1;
    }
    const char *topic = argv[1];
    const char *test_message = argv[2];

    Aws::SDKOptions options;
    Aws::InitAPI(options);

    rd_kafka_conf_t *conf = rd_kafka_conf_new();
    char errstr[512];

    rd_kafka_conf_set_dr_msg_cb(conf, dr_msg_cb);

    if (rd_kafka_conf_set(conf, "bootstrap.servers", brokers, errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
    {
        std::cerr << errstr << std::endl;
        Aws::ShutdownAPI(options);
        return 1;
    }

    rd_kafka_conf_set(conf, "security.protocol", "SASL_SSL", errstr, sizeof(errstr));
    rd_kafka_conf_set(conf, "sasl.mechanisms", "OAUTHBEARER", errstr, sizeof(errstr));
    rd_kafka_conf_set(conf, "debug", "all", errstr, sizeof(errstr));

    rd_kafka_conf_set_oauthbearer_token_refresh_cb(conf, [](rd_kafka_t *rk, const char *oauthbearer_config, void *opaque)
                                                   {
        try
        {
            AwsMskIamSigner signer("us-east-1");
            auto token = signer.generateToken();
            char errstr_token[512];
            rd_kafka_resp_err_t err = rd_kafka_oauthbearer_set_token(
                rk,
                token.token.c_str(),
                token.expiration_ms,
                "",
                nullptr, 0,
                errstr_token,
                sizeof(errstr_token));
            if (err != RD_KAFKA_RESP_ERR_NO_ERROR)
            {
                std::cerr << "Failed to set OAuth token: " << errstr_token << std::endl;
            }
        }
        catch (const std::exception &ex)
        {
            rd_kafka_oauthbearer_set_token_failure(rk, ex.what());
        } });

    rd_kafka_t *rk = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
    if (!rk)
    {
        std::cerr << "Failed to create producer: " << errstr << std::endl;
        Aws::ShutdownAPI(options);
        return 1;
    }
    rd_kafka_set_log_level(rk, SO_DEBUG);

    size_t len = strlen(test_message);
    rd_kafka_resp_err_t err = rd_kafka_producev(
        rk,
        RD_KAFKA_V_TOPIC(topic),
        RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
        RD_KAFKA_V_VALUE((void *)test_message, len),
        RD_KAFKA_V_OPAQUE(NULL),
        RD_KAFKA_V_END);
    if (err)
    {
        std::cerr << "Produce failed: " << rd_kafka_err2str(err) << std::endl;
    }
    else
    {
        std::cerr << "Enqueued message: " << test_message << std::endl;
    }
    rd_kafka_poll(rk, 0);

    std::cerr << "Flushing...\n";
    rd_kafka_flush(rk, 10000);

    if (rd_kafka_outq_len(rk) > 0)
        std::cerr << rd_kafka_outq_len(rk) << " message(s) were not delivered\n";

    rd_kafka_destroy(rk);
    Aws::ShutdownAPI(options);
    return 0;
}
