#ifndef DATA_PAYLOAD_H
#define DATA_PAYLOAD_H

#include <string>

class DataPayload {
public:
    virtual ~DataPayload() = default;

    // Allows the interface or reports to identify the type of data safely
    virtual std::string getPayloadType() const = 0;

    // memento 
    virtual DataPayload* clone() const = 0; 
};

#endif