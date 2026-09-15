// Spike for NeptuneGPS_Triton#98 phase 1: can the real AgIsoStack core be
// compiled and linked into a host test binary, driven by a fake CAN bus?
//
// Nothing here is production code. It includes the specific headers rather
// than the AgIsoStack.hpp umbrella (which pulls in the Teensy FlexCAN files),
// stands up the objects IsobusTcInterface stands up, and asks the DDOP for
// its binary -- the test #21 needs.
#include <cstdio>
#include <memory>
#include <vector>
#include <chrono>
#include <thread>

#include "can_hardware_interface_single_thread.hpp"
#include "can_hardware_plugin.hpp"
#include "can_internal_control_function.hpp"
#include "can_network_manager.hpp"
#include "can_partnered_control_function.hpp"
#include "isobus_device_descriptor_object_pool.hpp"
#include "isobus_standard_data_description_indices.hpp"
#include "isobus_task_controller_client.hpp"

// The five-method seam the stack was designed around. A test owns both
// queues: frames pushed into rx are read by the stack, frames the stack
// writes land in tx for assertions.
class FakeCanPlugin : public isobus::CANHardwarePlugin {
public:
    std::vector<isobus::CANMessageFrame> rx;
    std::vector<isobus::CANMessageFrame> tx;
    size_t rxPos = 0;
    bool   open_ = true;

    bool get_is_valid() const override { return open_; }
    void close() override { open_ = false; }
    void open() override { open_ = true; }

    bool read_frame(isobus::CANMessageFrame &frame) override {
        if (rxPos >= rx.size()) return false;
        frame = rx[rxPos++];
        return true;
    }

    bool write_frame(const isobus::CANMessageFrame &frame) override {
        tx.push_back(frame);
        return true;
    }
};

static bool gValueCommandSeen = false;

static bool on_value_command(std::uint16_t, std::uint16_t ddi, std::int32_t value, void *) {
    std::printf("   value command: DDI %u = %d\n", unsigned(ddi), int(value));
    gValueCommandSeen = true;
    return true;
}

static bool on_value_request(std::uint16_t, std::uint16_t, std::int32_t &value, void *) {
    value = 0;
    return true;
}

int main() {
    std::printf("== AgIsoStack native spike ==\n");

    // 1. Hardware layer on a fake bus.
    auto plugin = std::make_shared<FakeCanPlugin>();
    isobus::CANHardwareInterface::set_number_of_can_channels(1);
    isobus::CANHardwareInterface::assign_can_channel_frame_handler(0, plugin);
    const bool hwStarted = isobus::CANHardwareInterface::start();
    std::printf("1. hardware interface started: %s\n", hwStarted ? "yes" : "no");

    // 2. Our own control function, as main.cpp claims one.
    isobus::NAME ourName(0);
    ourName.set_arbitrary_address_capable(true);
    ourName.set_industry_group(2);
    ourName.set_device_class(6);
    ourName.set_function_code(static_cast<std::uint8_t>(isobus::NAME::Function::RateControl));
    ourName.set_identity_number(2);
    ourName.set_ecu_instance(0);
    ourName.set_function_instance(0);
    ourName.set_device_class_instance(0);
    ourName.set_manufacturer_code(1407);
    auto controlFunction = isobus::CANNetworkManager::CANNetwork.create_internal_control_function(ourName, 0, 0x1C);
    std::printf("2. internal control function: %s\n", controlFunction ? "created" : "NULL");

    // 3. A TC partner, the way IsobusTcInterface::Begin() does it.
    const isobus::NAMEFilter tcFilter(isobus::NAME::NAMEParameters::FunctionCode,
                                      static_cast<std::uint8_t>(isobus::NAME::Function::TaskController));
    auto partner = isobus::CANNetworkManager::CANNetwork.create_partnered_control_function(0, { tcFilter });
    std::printf("3. partnered control function: %s\n", partner ? "created" : "NULL");

    // 4. A DDOP, and its binary -- this is what a #21 regression test asserts on.
    auto ddop = std::make_shared<isobus::DeviceDescriptorObjectPool>();
    std::array<std::uint8_t, isobus::task_controller_object::DeviceObject::MAX_STRUCTURE_AND_LOCALIZATION_LABEL_LENGTH> localization;
    localization.fill(0xFF);
    const bool deviceAdded = ddop->add_device("Spike", "0.1.0", "001", "TC05",
                                              localization, std::vector<std::uint8_t>(), 0);
    const bool elementAdded = ddop->add_device_element("Plough", 1, 0,
                                                       isobus::task_controller_object::DeviceElementObject::Type::Device, 1);
    const bool dpdAdded = ddop->add_device_process_data(
        "Guidance line deviation",
        static_cast<std::uint16_t>(isobus::DataDescriptionIndex::GuidanceLineDeviation),
        0xFFFF,
        static_cast<std::uint8_t>(isobus::task_controller_object::DeviceProcessDataObject::PropertiesBit::MemberOfDefaultSet),
        static_cast<std::uint8_t>(isobus::task_controller_object::DeviceProcessDataObject::AvailableTriggerMethods::OnChange),
        2);
    std::printf("4. ddop add device/element/process data: %s/%s/%s\n",
                deviceAdded ? "ok" : "FAIL", elementAdded ? "ok" : "FAIL", dpdAdded ? "ok" : "FAIL");

    std::vector<std::uint8_t> binary;
    const bool generated = ddop->generate_binary_object_pool(binary);
    std::printf("5. generate_binary_object_pool: %s, %u bytes, %u objects\n",
                generated ? "ok" : "FAIL", unsigned(binary.size()), unsigned(ddop->size()));

    // 6. The TC client itself: the class the seam exists for.
    auto tcClient = std::make_shared<isobus::TaskControllerClient>(partner, controlFunction, nullptr);
    tcClient->configure(ddop, 0, 0, 1, false, false, true, false, true);
    tcClient->add_value_command_callback(on_value_command, nullptr);
    tcClient->add_request_value_callback(on_value_request, nullptr);
    tcClient->initialize(false);
    std::printf("6. task controller client: constructed, configured, initialised\n");

    // 7. Pump the stack the way loop() does. Nothing is connected, so this
    //    only has to not crash and not hang.
    for (int i = 0; i < 50; i++) {
        isobus::CANHardwareInterface::update();
        tcClient->update();
    }
    std::printf("7. 50 update cycles: survived, %u frames written to the bus\n",
                unsigned(plugin->tx.size()));

    // 8. The stack keeps its own time with std::chrono, so a test cannot mock
    //    it the way millis() is mocked. Find out what that costs: spin with
    //    real sleeps and see how long an address claim actually takes.
    const auto started = std::chrono::steady_clock::now();
    int msToClaim = -1;
    for (int i = 0; i < 400; i++) {
        isobus::CANHardwareInterface::update();
        tcClient->update();
        if (msToClaim < 0 && controlFunction->get_address_valid()) {
            msToClaim = int(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started).count());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    std::printf("8. after 2 s of real time: address 0x%02X, valid %s, claimed after %d ms, %u frames written\n",
                unsigned(controlFunction->get_address()),
                controlFunction->get_address_valid() ? "yes" : "no",
                msToClaim,
                unsigned(plugin->tx.size()));
    if (!plugin->tx.empty()) {
        const auto &f = plugin->tx.front();
        std::printf("   first frame: id 0x%08X dlc %u\n", unsigned(f.identifier), unsigned(f.dataLength));
    }

    tcClient->terminate();
    isobus::CANHardwareInterface::stop();
    std::printf("== spike complete ==\n");
    return (hwStarted && controlFunction && partner && generated && binary.size() > 0) ? 0 : 1;
}
