#include <random>
#include <string>
#include <thread>
#include <chrono>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include "mqtt/async_client.h"
#include "cxxopts.hpp"
#include <jwt-cpp/jwt.h>
#include <jansson.h>

const int QOS = 1;
const auto SAMPLE_INTERVAL = std::chrono::seconds(5);
const int MAX_BUFFERED_MSGS = 120;
const int N_RETRY_ATTEMPTS = 10;

class downlink_listener : public virtual mqtt::iaction_listener
{
	void on_failure(const mqtt::token &tok) override
	{
		std::cout << "Failure to subscribe to downlink topic";
		if (tok.get_message_id() != 0)
			std::cout << " for token: [" << tok.get_message_id() << "]" << std::endl;
		std::cout << std::endl;
	}

	void on_success(const mqtt::token &tok) override
	{
		std::cout << "Successfully subscribed to downlink topic";
		if (tok.get_message_id() != 0)
			std::cout << " for token: [" << tok.get_message_id() << "]" << std::endl;
		auto top = tok.get_topics();
		if (top && !top->empty())
			std::cout << "\tdownlink topic: '" << (*top)[0] << "', ..." << std::endl;
		std::cout << std::endl;
	}
};

/**
 * A callback class for use with the main MQTT client.
 */
class callback : public virtual mqtt::callback, public virtual mqtt::iaction_listener
{
	// Counter for the number of connection retries
	int retries_;
	std::string client_id_;
	std::string downlink_topic_;

	// The MQTT client
	mqtt::async_client &client_;
	// Options to use if we need to reconnect
	mqtt::connect_options &conn_opts_;
	// An action listener to display the result of downlinks.
	downlink_listener downlink_listener_;

	// This deomonstrates manually reconnecting to the broker by calling
	// connect() again. This is a possibility for an application that keeps
	// a copy of it's original connect_options, or if the app wants to
	// reconnect with different options.
	// Another way this can be done manually, if using the same options, is
	// to just call the async_client::reconnect() method.
	void reconnect()
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(2500));
		try
		{
			client_.connect(conn_opts_, nullptr, *this);
		}
		catch (const mqtt::exception &exc)
		{
			std::cerr << "Error: " << exc.what() << std::endl;
			exit(1);
		}
	}

	// Re-connection failure
	void on_failure(const mqtt::token &tok) override
	{
		std::cout << "Connection attempt failed" << std::endl;
		if (++retries_ > N_RETRY_ATTEMPTS)
			exit(1);
		reconnect();
	}

	// (Re)connection success
	// Either this or connected() can be used for callbacks.
	void on_success(const mqtt::token &tok) override {}

	void connection_lost(const std::string &cause) override
	{
		std::cout << "Client connection lost" << std::endl;
		if (!cause.empty())
			std::cout << "cause: " << cause << std::endl;
	}

	void connected(const std::string &cause) override
	{
		std::cout << "Client connected" << std::endl;
		std::cout << "Subscribing to topic '" << downlink_topic_ << "'\n"
				  << "\tfor client " << client_id_
				  << " using QoS" << QOS << std::endl;

		client_.subscribe(downlink_topic_, QOS, nullptr, downlink_listener_);
	}

	// Callback for when a message arrives.
	void message_arrived(mqtt::const_message_ptr msg) override
	{
		std::cout << "Downlink arrived" << std::endl;
		std::cout << "\ttopic: '" << msg->get_topic() << "'" << std::endl;
		std::cout << "\tpayload: '" << msg->to_string() << std::endl;

		// TODO implement downlink handling here
	}

	void delivery_complete(mqtt::delivery_token_ptr token) override {}

public:
	callback(mqtt::async_client &client, mqtt::connect_options &conn_opts, std::string &client_id, std::string &downlink_topic)
		: retries_(0), client_(client), conn_opts_(conn_opts), client_id_(client_id), downlink_topic_(downlink_topic), downlink_listener_() {}
};

std::string get_required_arg(const cxxopts::Options &options, const cxxopts::ParseResult &parsed_options, const std::string &option_name)
{
	if (parsed_options.count(option_name))
	{
		return parsed_options[option_name].as<std::string>();
	}
	else
	{
		std::cout << "the --" + option_name + " argument is required" << std::endl;
		std::cout << options.help() << std::endl;
		exit(0);
	}
}

std::string generate_token_es256(std::string &device_id, std::string &private_key_file_path, std::string &audience)
{
	std::ifstream priv_key_file(private_key_file_path);
	std::stringstream buffer;
	buffer << priv_key_file.rdbuf();
	auto es256_priv_key = buffer.str();
	if (!priv_key_file.good() || es256_priv_key.length() == 0)
	{
		std::cout << "the file" + private_key_file_path + " does not exist" << std::endl;
		exit(0);
	}

	return jwt::create()
		.set_type("JWT")
		.set_subject(device_id)
		.set_issued_at(std::chrono::system_clock::now())
		.set_expires_at(std::chrono::system_clock::now() + std::chrono::seconds{36000})
		.set_payload_claim("aud", jwt::claim(std::string{audience}))
		.sign(jwt::algorithm::es256("", es256_priv_key, "", ""));
}

std::string generate_token_rs256(std::string &device_id, std::string &private_key_file_path, std::string &audience)
{
	std::ifstream priv_key_file(private_key_file_path);
	std::stringstream buffer;
	buffer << priv_key_file.rdbuf();
	auto rs256_priv_key = buffer.str();
	if (!priv_key_file.good() || rs256_priv_key.length() == 0)
	{
		std::cout << "the file" + private_key_file_path + " does not exist" << std::endl;
		exit(0);
	}

	return jwt::create()
		.set_type("JWT")
		.set_subject(device_id)
		.set_issued_at(std::chrono::system_clock::now())
		.set_expires_at(std::chrono::system_clock::now() + std::chrono::seconds{36000})
		.set_payload_claim("aud", jwt::claim(std::string{audience}))
		.sign(jwt::algorithm::rs256("", rs256_priv_key, "", ""));
}

void send_uplink_loop(mqtt::async_client &client, std::string &uplink_topic_name)
{
	mqtt::topic topic(client, uplink_topic_name, QOS, true);

	// simulate data generation
	std::random_device random;
	std::mt19937 generator(random());
	std::uniform_int_distribution<> distribution(0, 100);

	// time at which to generate the next sample, starting now
	auto wake_ts = std::chrono::steady_clock::now();

	while (true)
	{
		std::this_thread::sleep_until(wake_ts);

		// generate data
		int temperature = distribution(generator);
		time_t now;
		time(&now);
		char buf[32];
		strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%S.000Z", gmtime(&now));

		json_t *root = json_object();
		json_object_set_new(root, "temperature", json_integer(temperature));
		json_object_set_new(root, "timestamp", json_string(buf));
		std::string payload = json_dumps(root, JSON_COMPACT);

		// publish sample to the topic
		std::cout << "Publishing" << std::endl;
		std::cout << "\ttopic: '" << uplink_topic_name << "'" << std::endl;
		std::cout << "\tpayload: '" << payload << std::endl;

		topic.publish(std::move(payload));

		wake_ts += SAMPLE_INTERVAL;
	}
}

int main(int argc, char **argv)
{
	try
	{
		cxxopts::Options options("ak_mqtt", "An example CLI for connecting to the akenza MQTT broker.");
		options.add_options()
			// MQTT hostname
			("s,mqtt_hostname", "MQTT hostname", cxxopts::value<std::string>()->default_value("mqtt.akenza.io"))
			// MQTT port
			("p,mqtt_port", "MQTT port", cxxopts::value<int>()->default_value("8883"))
			// Device ID
			("d,device_id", "The physical device id", cxxopts::value<std::string>())
			// MQTT username
			("u,mqtt_username", "MQTT username (only for uplink secret authentication)", cxxopts::value<std::string>())
			// MQTT password
			("r,mqtt_password", "MQTT password (only for uplink secret authentication)", cxxopts::value<std::string>())
			// Signing algorithm
			("a,algorithm", "Signing algorithm (ES256 or RS256)", cxxopts::value<std::string>()->default_value("ES256"))
			// Signing audience
			("c,audience", "Audience (e.g. akenza.io)", cxxopts::value<std::string>()->default_value("akenza.io"))
			// Private key
			("f,private_key_file", "Path to the private key", cxxopts::value<std::string>())
			// debug
			("v,verbose", "Enable verbose output", cxxopts::value<bool>()->default_value("false")->implicit_value("true"))
			// help
			("h,help", "Print usage");

		auto parsed_options = options.parse(argc, argv);
		if (parsed_options.count("help"))
		{
			std::cout << options.help() << std::endl;
			exit(0);
		}

		std::string mqtt_hostname = parsed_options["mqtt_hostname"].as<std::string>();
		int mqtt_port = parsed_options["mqtt_port"].as<int>();
		std::string address = "ssl://" + mqtt_hostname + ":" + std::to_string(mqtt_port);
		std::string audience_root = parsed_options["audience"].as<std::string>();
		std::string device_id = get_required_arg(options, parsed_options, "device_id");
		std::string algorithm = parsed_options["algorithm"].as<std::string>();
		std::string audience = "https://" + audience_root + "/devices/" + device_id;
		std::string private_key_file_path = get_required_arg(options, parsed_options, "private_key_file");
		// use /up/device/id/<deviceId> or /up/device/id/<deviceId>/<akenzaTopic> to send uplinks
		std::string uplink_topic_name = "/up/device/id/" + device_id + "/measurements";
		// use /down/device/id/<deviceId> or /down/device/id/<deviceId>/<subTopic> to receive downlinks
		std::string downlink_topic_name = "/down/device/id/" + device_id + "/#";

		auto ssl_opts = mqtt::ssl_options_builder()
							.verify(true)
							.error_handler([](const std::string &msg)
										   { std::cerr << "SSL Error: " << msg << std::endl; })
							.finalize();

		// TODO add support for uplink secret authentication
		std::string token;
		if (algorithm == "ES256")
		{
			token = generate_token_es256(device_id, private_key_file_path, audience);
		}
		else if (algorithm == "RS256")
		{
			token = generate_token_rs256(device_id, private_key_file_path, audience);
		}
		else
		{
			std::cout << "the algorithm: " + algorithm + " is not supported" << std::endl;
			exit(0);
		}

		auto connect_opts = mqtt::connect_options_builder()
								.keep_alive_interval(MAX_BUFFERED_MSGS * SAMPLE_INTERVAL)
								.clean_session(true)
								.automatic_reconnect(true)
								.password(token)
								.user_name(device_id)
								.ssl(std::move(ssl_opts))
								.finalize();

		mqtt::async_client client(address, device_id, MAX_BUFFERED_MSGS);
		callback cb(client, connect_opts, device_id, downlink_topic_name);
		client.set_callback(cb);

		try
		{
			// Connect to the MQTT broker
			std::cout << "Connecting to server '" << address << "'..." << std::flush;
			client.connect(connect_opts)->wait();

			// TODO reconnect on token expiration
			send_uplink_loop(client, uplink_topic_name);

			// disconnect
			std::cout
				<< "Disconnecting..." << std::endl;
			client.disconnect()->wait();
		}
		catch (const mqtt::exception &exc)
		{
			std::cerr << exc.what() << std::endl;
			return 1;
		}
	}
	catch (const cxxopts::exceptions::exception &exc)
	{
		std::cerr << exc.what() << std::endl;
		return 1;
	}

	return 0;
}
