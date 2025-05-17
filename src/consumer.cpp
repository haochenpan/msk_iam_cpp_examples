#include <aws/core/Aws.h>
#include "AwsMskIamSigner.h"
#include <csignal>
#include <iostream>
#include <librdkafka/rdkafka.h>

static volatile sig_atomic_t run = 1;
static void stop(int) { run = 0; }

static int is_printable(const char *buf, size_t size)
{
    for (size_t i = 0; i < size; i++)
        if (!isprint((int)buf[i]))
            return 0;
    return 1;
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <topic> <group.id>\n";
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
        std::cerr << "OCTOPUS_BOOTSTRAP_SERVERS not set\n";
        return 1;
    }

    const char *topic = argv[1], *group_id = argv[2]; //, *group_protocol = argv[3];

    Aws::SDKOptions options;
    Aws::InitAPI(options);

    rd_kafka_conf_t *conf = rd_kafka_conf_new();
    char errstr[512];

    rd_kafka_conf_set(conf, "bootstrap.servers", brokers, errstr, sizeof(errstr));
    rd_kafka_conf_set(conf, "group.id", group_id, errstr, sizeof(errstr));
    // rd_kafka_conf_set(conf, "group.protocol", group_protocol, errstr, sizeof(errstr));
    rd_kafka_conf_set(conf, "security.protocol", "SASL_SSL", errstr, sizeof(errstr));
    rd_kafka_conf_set(conf, "sasl.mechanisms", "OAUTHBEARER", errstr, sizeof(errstr));
    rd_kafka_conf_set(conf, "auto.offset.reset", "earliest", errstr, sizeof(errstr));

    rd_kafka_conf_set_oauthbearer_token_refresh_cb(conf, [](rd_kafka_t *rk, const char *, void *)
                                                   {
        try {
            AwsMskIamSigner signer("us-east-1");
            auto token = signer.generateToken();
            char err[512];
            rd_kafka_oauthbearer_set_token(rk, token.token.c_str(), token.expiration_ms, "", nullptr, 0, err, sizeof(err));
        } catch (const std::exception &ex) {
            rd_kafka_oauthbearer_set_token_failure(rk, ex.what());
        } });

    rd_kafka_t *rk = rd_kafka_new(RD_KAFKA_CONSUMER, conf, errstr, sizeof(errstr));
    if (!rk)
    {
        std::cerr << "Failed to create consumer: " << errstr << "\n";
        Aws::ShutdownAPI(options);
        return 1;
    }

    rd_kafka_poll_set_consumer(rk);

    rd_kafka_topic_partition_list_t *topics = rd_kafka_topic_partition_list_new(1);
    rd_kafka_topic_partition_list_add(topics, topic, RD_KAFKA_PARTITION_UA);
    rd_kafka_subscribe(rk, topics);
    rd_kafka_topic_partition_list_destroy(topics);
    signal(SIGINT, stop);

    while (run)
    {
        rd_kafka_message_t *msg = rd_kafka_consumer_poll(rk, 100);
        if (!msg)
            continue;

        if (msg->err)
        {
            std::cerr << "Consumer error: " << rd_kafka_message_errstr(msg) << "\n";
        }
        else if (msg->payload)
        {
            std::cout << "Message on " << rd_kafka_topic_name(msg->rkt)
                      << " [" << msg->partition << "] at offset " << msg->offset << ":\n";
            if (is_printable((char *)msg->payload, msg->len))
                std::cout << "  Value: " << std::string((char *)msg->payload, msg->len) << "\n";
            else
                std::cout << "  Value: (" << msg->len << " bytes)\n";
        }

        rd_kafka_message_destroy(msg);
    }

    rd_kafka_consumer_close(rk);
    rd_kafka_destroy(rk);
    Aws::ShutdownAPI(options);
    return 0;
}
