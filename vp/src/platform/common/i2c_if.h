#ifndef RISCV_VP_I2C_IF_H
#define RISCV_VP_I2C_IF_H

#include <map>
#include <queue>
#include <systemc>

#include "core/common/irq_if.h"
#include "util/tlm_map.h"

/* generic i2c device interface */
class I2C_Device_IF {
   public:
   
    virtual bool write(uint8_t data) = 0;
    virtual uint8_t read() = 0;
};

/* i2c host interface */
class I2C_IF {

    std::map<uint8_t, I2C_Device_IF*> devices;

    public: 

    void register_device(uint8_t addr, I2C_Device_IF* device) {
        devices.insert(std::pair<const uint32_t, I2C_Device_IF*>(addr, device));
    }

    void unregister_device(uint8_t addr) {
        devices.erase(addr);
    }

    I2C_Device_IF *get_device(uint8_t addr) {
        auto device = devices.find(addr);
        if (device == devices.end()) {
            return nullptr;
        }
        return device->second;
    }

    bool write(uint8_t addr, uint8_t data) {
        I2C_Device_IF *device = get_device(addr);
        bool ack = false;
		if (device == nullptr) {
			std::cerr << "I2C: WARNING: Write to unregistered adress" << addr << std::endl;
			return false;
		}
        ack = device->write(data);
        if (!ack) {
            std::cerr << "I2C: WARNING: Write to device " << addr << " failed" << std::endl;
            return false;
        }
		return true;
    }

    bool read(uint8_t addr, uint8_t &data) {
        I2C_Device_IF *device = get_device(addr);
        if (device == nullptr) {
            std::cerr << "I2C: WARNING: Read from unregistered adress" << addr << std::endl;
            return false;
        }
        data = device->read();
        return true;
    }
    
};

#endif /* RISCV_VP_I2C_IF_H */