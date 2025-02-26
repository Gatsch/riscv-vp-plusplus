#ifndef RISCV_VP_FU540_I2C_H
#define RISCV_VP_FU540_I2C_H

#include <systemc>

class FU540_I2C : public sc_core::sc_module, public I2C_IF {
   public:
    const int *interrupts = nullptr;
    interrupt_gateway *plic = nullptr;

    tlm_utils::simple_target_socket<FU540_I2C> tsock;

    FU540_I2C(const sc_core::sc_module_name &name, const int *interrupts);
    ~FU540_I2C(void);

    SC_HAS_PROCESS(FU540_I2C);


    private:

    uint8_t reg_prer_lo = 0;
    uint8_t reg_prer_hi = 0;
    uint8_t reg_ctr = 0;
    uint8_t reg_txr = 0;
    uint8_t reg_rxr = 0;
    uint8_t reg_cr = 0;
    uint8_t reg_sr = 0;

	vp::map::LocalRouter router = {"FU540_GPIO"};

    enum {
        // Clock Prescale register lo-byte
        REG_PRER_LO = 0x00,
        // Clock Prescale register hi-byte
        REG_PRER_HI = 0x01,
        // Control register
        REG_CTR = 0x02,
        // Transmit Recei register
        REG_TXR_RXR = 0x03,
        // Command Status register
        REG_CR_SR = 0x04,
    }


    FU540_I2C::FU540_I2C(const sc_core::sc_module_name &, const int *interrupts) {
        tsock.register_b_transport(this, &FU540_I2C::transport);
        router
            .add_register_bank({
                {REG_PRER_LO, &reg_prer_lo},
                {REG_PRER_HI, &reg_prer_hi},
                {REG_CTR, &reg_ctr},
                {REG_TXR_RXR, &reg_txr},
                {REG_CR_SR, &reg_cr},
            })
            .register_handler(this, &FU540_I2C::register_update_callback);

    }

    void FU540_I2C::transport(tlm::tlm_generic_payload &trans, sc_core::sc_time &delay) {
	    router.transport(trans, delay);
    }

    void FU540_I2C::register_update_callback(const vp::map::register_access_t &r) {
	r.fn();
    }


}

#endif  // RISCV_VP_FU540_I2C_H