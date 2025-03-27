
#include "fu540_i2c.h"

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
        switch (r.addr) {
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
            if (r.nv & I2C_CR_IACK) {
                interruptFlag = false;
            } 
            if (r.nv & I2C_CR_WR) {
                //TODO: Flag is immideatly cleared
                transferInProgress = true;
                if (r.nv & I2C_CR_STA) {
                    rxack = !I2C_IF::start((reg_txr & I2C_TX_ADDR) >> 1);
                } else {
                    uint8_t data = reg_txr;
                    rxack = !I2C_IF::write(data);
                }
                transferInProgress = false;
                triggerInterrupt();
                if (r.nv & I2C_CR_STO) {
                    I2C_IF::stop();
                }
            } else if (r.nv & I2C_CR_RD) {
                if (r.nv & I2C_CR_STA) {
                    //TODO: Error??
                } else {
                    transferInProgress = true;
                    uint8_t data;
                    I2C_IF::read(data);
                    reg_rxr = data;
                    transferInProgress = false;
                    triggerInterrupt();
                    if (r.nv & I2C_CR_STO) {
                        I2C_IF::stop();
                    }
                }
            }
            sendACK = ~(r.nv & I2C_CR_ACK); //TODO: what to do with ACK
            break;
        default:
            //TODO: possible error
            break;
        }
    } else if (r.read) {
        reg_sr = getStatusRegister(); //update status register before possible read
        r.fn();
    }
}

void FU540_I2C::triggerInterrupt() {
    if (plic != nullptr && interrupt_enabled && !interruptFlag) {
        plic->gateway_trigger_interrupt(interrupt);
    }
    interruptFlag = true;
}

uint8_t FU540_I2C::getStatusRegister() {
    uint8_t status = 0;
    if (rxack)              status |= I2C_SR_RXACK;
    if (busy)               status |= I2C_SR_BUSY;
    if (arbitrationLost)    status |= I2C_SR_AL;
    if (transferInProgress) status |= I2C_SR_TIP;
    if (interruptFlag)      status |= I2C_SR_IF;
    return status;
}
