#ifndef ROLE_H
#define ROLE_H
#include <string>

// A responsibility a participant can have
class Role {
private:
    std::string roleName;
public:
    Role(const std::string& name);
    std::string getName() const;
    bool matches(const std::string& otherRole) const;
};
#endif
