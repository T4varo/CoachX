#pragma once
#include <exception>
#include <string>

class MultipleHookException : public std::exception {
public:
	MultipleHookException(const std::string eventName) {
		this->msg = "Trying to hook " + eventName + " multiple times!";
	}
	virtual const char* what() const throw() {
		return msg.c_str();
	}
private:
	std::string msg;
};
