#ifndef RISCV_VP_FU540_I2C_H
#define RISCV_VP_FU540_I2C_H

#include <systemc>
#include "util/tlm_map.h"
#include "i2c_if.h"

class FU540_I2C : public sc_core::sc_module, public I2C_IF {
public:
    const int interrupt = 0;
    interrupt_gateway *plic = nullptr;

    tlm_utils::simple_target_socket<FU540_I2C> tsock;

    FU540_I2C(const sc_core::sc_module_name &name, const int interrupt);

    SC_HAS_PROCESS(FU540_I2C);

private:
    bool enabled = false;
    bool interrupt_enabled = false;

    uint32_t reg_prer_lo = 0xFF;
    uint32_t reg_prer_hi = 0xFF;
    uint32_t reg_ctr = 0x00;
    uint32_t reg_txr = 0x00;
    uint32_t reg_rxr = 0x00;
    uint32_t reg_sr = 0x00;

    bool sendACK = false;
    bool rxack = false;
    bool busy = false;
    bool arbitrationLost = false;
    bool transferInProgress = false;
    bool interruptFlag = false;

    vp::map::LocalRouter router = {"FU540_I2C"};

    enum {
        // Clock Prescale register lo-byte
        REG_PRER_LO = 0x00,
        // Clock Prescale register hi-byte
        REG_PRER_HI = 0x01,
        // Control register
        REG_CTR = 0x02,
        // Transmit & Receive register
        REG_TXR_RXR = 0x03,
        // Command & Status register
        REG_CR_SR = 0x04,
    };

    #define I2C_CTR_EN (1 << 7)
    #define I2C_CTR_IEN (1 << 6)
    #define I2C_TX_ADDR (0xFF ^ 1)
    #define I2C_TX_WR 1
    #define I2C_CR_STA (1 << 7)
    #define I2C_CR_STO (1 << 6)
    #define I2C_CR_RD (1 << 5)
    #define I2C_CR_WR (1 << 4)
    #define I2C_CR_ACK (1 << 3)
    #define I2C_CR_IACK (1)
    #define I2C_SR_RXACK (1 << 7)
    #define I2C_SR_BUSY (1 << 6)
    #define I2C_SR_AL (1 << 5)
    #define I2C_SR_TIP (1 << 1)
    #define I2C_SR_IF (1)

    void transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay);
    void register_update_callback(const vp::map::register_access_t &r);
    uint8_t get_status_register();
};

FU540_I2C::FU540_I2C(const sc_core::sc_module_name &name, const int interrupt)
    : sc_module(name), interrupt(interrupt), router("FU540_I2C") {
    tsock.register_b_transport(this, &FU540_I2C::transport);
    router
        .add_register_bank({
            {REG_PRER_LO, &reg_prer_lo},
            {REG_PRER_HI, &reg_prer_hi},
            {REG_CTR, &reg_ctr},
            {REG_TXR_RXR, &reg_rxr},
            {REG_CR_SR, &reg_sr},
        })
        .register_handler(this, &FU540_I2C::register_update_callback);
}

void FU540_I2C::transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
    router.transport(trans, delay);
}

void FU540_I2C::register_update_callback(const vp::map::register_access_t &r) {

    if(r.write) {
        switch (r.addr) { //TODO: check if addr is correct -> alternative switch to pointer
        case REG_PRER_LO: 
        case REG_PRER_HI:
            r.fn();
            break;
        case REG_CTR:
            r.fn();
            enabled = reg_ctr & I2C_CTR_EN;
            interrupt_enabled = reg_ctr & I2C_CTR_IEN;
            break;
        case REG_TXR_RXR:
            //r.vptr = &reg_txr;
            reg_txr = r.nv;
            //r.fn();
            break;
        case REG_CR_SR:
            if (!enabled) {
                return;
            }
        
            if (r.nv & I2C_CR_WR) {
                
                if (r.nv & I2C_CR_STA) {
                    //TODO: Flag is immideatly cleared??
                    transferInProgress = true;
                    rxack = !I2C_IF::start((reg_txr & I2C_TX_ADDR) >> 1);
                    transferInProgress = false; //TODO: interrupt
                } else {
                    transferInProgress = true;
                    uint8_t data = reg_txr;
                    rxack = !I2C_IF::write(data);
                    transferInProgress = false; //TODO: interrupt
                    if (r.nv & I2C_CR_STO) {
                        I2C_IF::stop();
                    }
                }

            } else if (r.nv & I2C_CR_RD) {
                if (r.nv & I2C_CR_STA) {
                    //TODO: Error??
                } else {
                    transferInProgress = true;
                    uint8_t data;
                    I2C_IF::read(data);
                    reg_rxr = data;
                    transferInProgress = false; //TODO: interrupt
                    if (r.nv & I2C_CR_STO) {
                        I2C_IF::stop();
                    }
                }
            }

            sendACK = ~(r.nv & I2C_CR_ACK); //TODO: what to do with ACK
            break;
        default:
            //TODO: r.fn() or error?
            break;
        }
    } else if (r.read) {
        reg_sr = get_status_register(); //update status register before possible read
        r.fn();
    }
}

uint8_t FU540_I2C::get_status_register() {
    uint8_t status = 0;

    if (rxack)              status |= I2C_SR_RXACK;
    if (busy)               status |= I2C_SR_BUSY;
    if (arbitrationLost)    status |= I2C_SR_AL;
    if (transferInProgress) status |= I2C_SR_TIP;
    if (interruptFlag)      status |= I2C_SR_IF;

    return status;
}

#endif  // RISCV_VP_FU540_I2C_H