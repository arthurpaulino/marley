﻿// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Common/Common.h"
#include "Common/Event.h"
#include "Common/Flag.h"
#include "Common/SPSCQueue.h"
#include "Core/HW/Wiimote.h"
#include "Core/HW/WiimoteCommon/WiimoteConstants.h"
#include "Core/HW/WiimoteCommon/WiimoteHid.h"
#include "Core/HW/WiimoteCommon/WiimoteReport.h"

class PointerWrap;

namespace WiimoteReal
{
using WiimoteCommon::MAX_PAYLOAD;

// Includes HID "type" header byte.
using Report = std::vector<u8>;
constexpr int REPORT_HID_HEADER_SIZE = 1;

constexpr u32 WIIMOTE_DEFAULT_TIMEOUT = 1000;

// The 4 most significant bits of the first byte of an outgoing command must be
// 0x50 if sending on the command channel and 0xA0 if sending on the interrupt
// channel. On Mac and Linux we use interrupt channel; on Windows, command.
#ifdef _WIN32
constexpr u8 WR_SET_REPORT = 0x50;
#else
constexpr u8 WR_SET_REPORT = 0xA0;
#endif

constexpr u8 BT_INPUT = 0x01;
constexpr u8 BT_OUTPUT = 0x02;

class Wiimote : public WiimoteCommon::HIDWiimote
{
public:
  Wiimote(const Wiimote&) = delete;
  Wiimote& operator=(const Wiimote&) = delete;
  Wiimote(Wiimote&&) = delete;
  Wiimote& operator=(Wiimote&&) = delete;

  virtual ~Wiimote() {}
  // This needs to be called in derived destructors!
  void Shutdown();

  virtual std::string GetId() const = 0;

  bool GetNextReport(Report* report);

  bool IsBalanceBoard();

  void InterruptDataOutput(const u8* data, const u32 size) override;
  void Update() override;
  void EventLinked() override;
  void EventUnlinked() override;
  bool IsButtonPressed() override;

  void EmuStop();

  void EmuResume();
  void EmuPause();

  bool Connect(int index);
  void Prepare();

  virtual bool IsConnected() const = 0;

  void QueueReport(WiimoteCommon::OutputReportID rpt_id, const void* data, unsigned int size);

  template <typename T>
  void QueueReport(const T& report)
  {
    QueueReport(report.REPORT_ID, &report, sizeof(report));
  }

  int GetIndex() const;
  u16 buttons_hex;
  
protected:
  Wiimote();

  int m_index = 0;
  Report m_last_input_report;

  // If true, the Wiimote will be really disconnected when it is disconnected by Dolphin.
  // In any other case, data reporting is not paused to allow reconnecting on any button press.
  // This is not enabled on all platforms as connecting a Wiimote can be a pain on some platforms.
  bool m_really_disconnect = false;

private:
  void Read();
  bool Write();

  void StartThread();
  void StopThread();

  bool PrepareOnThread();

  void ResetDataReporting();

  virtual void EnablePowerAssertionInternal() {}
  virtual void DisablePowerAssertionInternal() {}

  virtual bool ConnectInternal() = 0;
  virtual void DisconnectInternal() = 0;

  Report& ProcessReadQueue(bool repeat_last_data_report);
  void ClearReadQueue();
  void WriteReport(Report rpt);

  virtual int IORead(u8* buf) = 0;
  virtual int IOWrite(u8 const* buf, size_t len) = 0;
  virtual void IOWakeup() = 0;

  void ThreadFunc();

  bool m_is_linked = false;

  // We track the speaker state to convert unnecessary speaker data into rumble reports.
  bool m_speaker_enable = false;
  bool m_speaker_mute = false;

  // And we track the rumble state to drop unnecessary rumble reports.
  bool m_rumble_state = false;

  std::thread m_wiimote_thread;
  // Whether to keep running the thread.
  Common::Flag m_run_thread;
  // Whether to call PrepareOnThread.
  Common::Flag m_need_prepare;
  // Triggered when the thread has finished ConnectInternal.
  Common::Event m_thread_ready_event;

  Common::SPSCQueue<Report> m_read_reports;
  Common::SPSCQueue<Report> m_write_reports;
};

class WiimoteScannerBackend
{
public:
  virtual ~WiimoteScannerBackend() = default;
  virtual bool IsReady() const = 0;
  virtual void FindWiimotes(std::vector<Wiimote*>&, Wiimote*&) = 0;
  // function called when not looking for more Wiimotes
  virtual void Update() = 0;
};

enum class WiimoteScanMode
{
  DO_NOT_SCAN,
  CONTINUOUSLY_SCAN,
  SCAN_ONCE
};

class WiimoteScanner
{
public:
  WiimoteScanner() = default;
  void StartThread();
  void StopThread();
  void SetScanMode(WiimoteScanMode scan_mode);

  bool IsReady() const;

private:
  void ThreadFunc();
  void PoolThreadFunc();

  std::vector<std::unique_ptr<WiimoteScannerBackend>> m_backends;
  mutable std::mutex m_backends_mutex;

  std::thread m_scan_thread;
  Common::Flag m_scan_thread_running;
  Common::Event m_scan_mode_changed_event;
  std::atomic<WiimoteScanMode> m_scan_mode{WiimoteScanMode::DO_NOT_SCAN};
};

// Mutex is recursive as ControllerInterface may call AddWiimoteToPool within ProcessWiimotePool.
extern std::recursive_mutex g_wiimotes_mutex;
extern std::unique_ptr<Wiimote> g_wiimotes[MAX_BBMOTES];

void AddWiimoteToPool(std::unique_ptr<Wiimote>);

bool IsValidDeviceName(const std::string& name);
bool IsBalanceBoardName(const std::string& name);
bool IsNewWiimote(const std::string& identifier);

void HandleWiimoteSourceChange(unsigned int wiimote_number);

#ifdef ANDROID
void InitAdapterClass();
#endif

void HandleWiimotesInControllerInterfaceSettingChange();
void ProcessWiimotePool();

}  // namespace WiimoteReal
