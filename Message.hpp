#pragma once

#include <string>

class Message {
	public:
		enum TargetType {
			CLIENT,
			CHANNEL,
			SERVER
		};

		Message(const std::string &msg, TargetType _targetType, const std::string &target);
		~Message();

	private:


		Message();
		Message(const Message &other);
		Message &operator=(const Message &other);

		std::string	_msg;
		TargetType	_targetType;
		std::string	_target; // could be a client nick, channel name, or server name
};
