#ifndef RISCV_VP_I2C_IF_H
#define RISCV_VP_I2C_IF_H

#include <map>
#include <queue>
#include <systemc>

#include "core/common/irq_if.h"
#include "util/tlm_map.h"

/* generic i2c device interface */
class I2C_Device_IF {
    friend class I2C_IF;
   
   protected:
    virtual bool start() = 0;
    virtual bool write(uint8_t data) = 0;
    virtual bool read(uint8_t &data) = 0;
    virtual bool stop() = 0;
};

/* i2c host interface */
class I2C_IF {

    std::map<uint8_t, I2C_Device_IF*> devices;
    I2C_Device_IF *device;

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

    bool start(uint8_t addr) {
        device = get_device(addr);
        if (device == nullptr) {
            std::cerr << "I2C: WARNING: Device not registered" << addr << std::endl;
            return false;
        }
        bool ack = device->start();
        if (!ack) {
            std::cerr << "I2C: WARNING: Device not responding" << addr << std::endl;
        }
        return ack;
    }

    bool write(uint8_t data) {
        bool ack = false;
        if (device == nullptr) {
            std::cerr << "I2C: WARNING: No device addressed" << std::endl;
            return false;
        }
        ack = device->write(data);
        if (!ack) {
            std::cerr << "I2C: WARNING: Write failed" << std::endl;
        }
		return ack;
    }

    bool read(uint8_t &data) {
        bool ack;
        if (device == nullptr) {
            std::cerr << "I2C: WARNING: No device addressed" << std::endl;
            return false;
        }
        ack = device->read(data);
        if (!ack) {
            std::cerr << "I2C: WARNING: Read failed" << std::endl;
        }
        return ack;
    }
    
    void stop() {
        if (device == nullptr) {
            std::cerr << "I2C: WARNING: No device addressed" << std::endl;
            return;
        }
        device->stop();
        device = NULL;
    }
};

#endif /* RISCV_VP_I2C_IF_H */