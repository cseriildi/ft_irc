#include "Message.hpp"
#include <string>

Message::Message(const std::string &msg, TargetType targetType, const std::string &target) :
	_msg(msg), _targetType(targetType), _target(target) {}

Message::~Message() {}
