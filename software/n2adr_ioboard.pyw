#!/usr/bin/env python

import sys, time, threading, select, traceback, queue
from functools import partial
import tkinter as tk
from tkinter import ttk
import hermeslite

VERSION = 'Version 1.1'

def no_port_1024_discover(ifaddr=None, verbose=2):
  return hermeslite.discover_by_port(ifaddr, 1025, verbose)

hermeslite.discover = no_port_1024_discover

class CommThread(threading.Thread):
  def __init__(self, app):
    threading.Thread.__init__(self, name="CommThread")
    self.app = app
    self.HL = None
    self.have_ioboard = False
    self.poll_state = 0
    self.comm_time = 0
    self.useBandVolts = 0
    self.useUartTx = 0
    self.useUartRx = 0
    self.Registers = bytearray(256)
    self.write_queue = queue.SimpleQueue()
    self.doQuit = threading.Event()
    self.doQuit.clear()
  def run(self):
    while not self.doQuit.is_set():
      try:
        if self.HL:
          self.Purge()
          self.KeepAlive()
          if self.have_ioboard:
            self.PollIoBoard()
        else:	# search for the Hermes Lite2
          self.SearchHL2()
        time.sleep(0.1)
      except:
        traceback.print_exc()
  def Purge(self):
    try:
      sock = self.HL.sock
      ready = select.select([sock], [], [], 0)	# Throw away available input
      while ready[0]:
        data, ip_port = sock.recvfrom(60)
    except:
      traceback.print_exc()
  def stop(self):
    self.doQuit.set()
  def SearchHL2(self):
    if time.time() - self.comm_time > 2.0:
      if self.app.known_ip:
        self.app.IP.set("  Trying...")
        hl = hermeslite.HermesLite((self.app.known_ip, 1025))
        if hl.response():
          self.HL = hl
      else:
        self.app.IP.set("  Searching...")
        self.HL = hermeslite.discover_first(0)
      if self.HL:
        self.app.IP.set("%s:%d" % (self.HL.ip, self.HL.port))
        time.sleep(0.3)
        self.Purge()
        if self.HL.read_ioboard_rom() == 0xF1:
          self.have_ioboard = True
        elif self.HL.read_ioboard_rom() == 0xF1:
          self.have_ioboard = True
      self.comm_time = time.time()
  def KeepAlive(self):
    if time.time() - self.comm_time > 5.0:
      resp = self.HL.response()
      if resp:
        self.app.temp.set("%.1f" % resp.temperature)
      else:
        self.HL = None
        self.have_ioboard = False
        self.Registers = bytearray(256)
        self.app.temp.set('')
      self.comm_time = time.time()
  def ReadBoard(self, addr, fullresponse=False):
    """Read from N2ADR IO board Pico via i2c."""
    addr = addr & 0xff
    cmd = bytes([0x07, 0x1d, addr, 0x00])
    res = self.HL.command(0x3d, cmd, sleep=0, timeout=0.5, attempts=2)
    if res:
      self.comm_time = time.time()
      if fullresponse:
        return res
      r = res.response_data
      return bytes((r & 0xFF, r >> 8 & 0xFF, r >> 16 & 0xFF, r >> 24 & 0xFF))
  def WriteBoard(self, addr, data):
    addr = addr & 0xFF
    data = data & 0xFF
    self.write_queue.put((addr, data))
  def PollIoBoard(self):
    if time.time() - self.comm_time > 0.1:
      app = self.app
      Reg = self.Registers
      self.Purge()
      if not self.write_queue.empty():
        try:
          addr, data = self.write_queue.get_nowait()
        except:
          traceback.print_exc()
        else:
          cmd = bytes([0x06, 0x1d, addr, data])
          res = self.HL.command(0x3d, cmd, sleep=0, timeout=0.5, attempts=2)
          if res and res.response_data & 0xFFFFFF == 0x1d << 16 | addr << 8 | data:
            self.comm_time = time.time()
          return
      if self.poll_state == 0:		# Registers 0, 1, 2, 3
        read_i2c = self.ReadBoard(0, True)
        if read_i2c:
          r = read_i2c.response_data
          r =  bytes((r & 0xFF, r >> 8 & 0xFF, r >> 16 & 0xFF, r >> 24 & 0xFF))
          Reg[0:4] = r
          self.app.temp.set("%.1f" % read_i2c.temperature)
      elif self.poll_state == 1:	# Registers 4, 5, 6, 7
        read_i2c = self.ReadBoard(4)
        if read_i2c:
          Reg[4:4 + 4] = read_i2c
        tx = Reg[0] << 32 | Reg[1] << 24 | Reg[2] << 16 | Reg[3] << 8 | Reg[4]
        app.tx_freq.set(FreqFormatter(tx))
      elif self.poll_state == 2:	# Registers 167, 168, 169, 170
        read_i2c = self.ReadBoard(167)
        if read_i2c:	# REG_STATUS, REG_IN_PINS, REG_OUT_PINS, GPIO00_HPF 
          Reg[167:167 + 4] = read_i2c
        # Status
        app.Sw5.set(bool(Reg[167] & 0x01))
        app.Sw12.set(bool(Reg[167] & 0x02))
        self.useUartRx = Reg[167] & 0x04
        # Input pins
        self.useBandVolts = Reg[168] & 0x80
        self.useUartTx = Reg[168] & 0x40
        app.In5.set(bool(Reg[168] & 0x20))
        app.In4.set(bool(Reg[168] & 0x10))
        app.In3.set(bool(Reg[168] & 0x08))
        app.In2.set(bool(Reg[168] & 0x04))
        if self.useUartRx:
          app.In1.set(0)
        else:
          app.In1.set(bool(Reg[168] & 0x02))
        app.EXTTR.set(bool(Reg[168] & 0x01))
        # Output pins
        if self.useBandVolts and app.ctrlOut8.winfo_ismapped():
          app.ctrlOut8.grid_remove()
        if self.useUartTx and app.ctrlOut1.winfo_ismapped():
          app.ctrlOut1.grid_remove()
        if self.useUartRx and app.ctrlIn1.winfo_ismapped():
          app.ctrlIn1.grid_remove()
        if not self.useBandVolts:
          app.Out8.set(bool(Reg[169] & 0x80))
        app.Out7.set(bool(Reg[169] & 0x40))
        app.Out6.set(bool(Reg[169] & 0x20))
        app.Out5.set(bool(Reg[169] & 0x10))
        app.Out4.set(bool(Reg[169] & 0x08))
        app.Out3.set(bool(Reg[169] & 0x04))
        app.Out2.set(bool(Reg[169] & 0x02))
        if not self.useUartTx:
          app.Out1.set(bool(Reg[169] & 0x01))
      elif self.poll_state == 3:	# Read register app.reg_index
        index = app.reg_index.get()	# a string, maybe ""
        try:
          index = int(index)
        except:
          app.reg_value.set('')
        else:
          if 0 <= index <= 252:
            read_i2c = self.ReadBoard(index)
            if read_i2c:
              Reg[index:index + 4] = read_i2c
              value = Reg[index]
              app.reg_value.set("%3d, 0x%02X" % (value, value))
          else:
            app.reg_value.set('')
      elif self.poll_state == 4:	# Read GPIO pin app.gpio_index
        index = app.gpio_index.get()	# a string, maybe ""
        try:
          index = int(index)
        except:
          app.gpio_value.set('')
        else:
          if 0 <= index <= 28:
            index += 170
            read_i2c = self.ReadBoard(index)
            if read_i2c:
              Reg[index:index + 4] = read_i2c
              value = Reg[index]
              app.gpio_value.set("%3d, 0x%02X" % (value, value))
          else:
            app.gpio_value.set('')
      elif self.poll_state == 5:	# named variable
        name = app.var_name1.get()
        if name == "Band Volts":
          read_i2c = self.ReadBoard(178)
          if read_i2c:
            Reg[178:178 + 4] = read_i2c
          if self.useBandVolts:
            v = Reg[178] / 255.0 * 5.0
            app.var_value1.set("%.3f volts" % v)
          else:
            app.var_value1.set('Not available')
        elif name == "Fan Volts":
          read_i2c = self.ReadBoard(12)
          if read_i2c:
            Reg[12:12 + 4] = read_i2c
          v =  3.7 * Reg[12] / 255.0 * 3.3 - 0.7
          if v < 0:
            v = 0.0
          app.var_value1.set("%.1f volts" % v)
        elif name == "ADC0":
          read_i2c = self.ReadBoard(25)
          if read_i2c:
            Reg[25:25 + 4] = read_i2c
          v = Reg[25] << 8 | Reg[26]
          v = v / 4095.0 * 3.0
          app.var_value1.set("%.3f volts" % v)
        elif name == "ADC1":
          read_i2c = self.ReadBoard(27)
          if read_i2c:
            Reg[27:27 + 4] = read_i2c
          v = Reg[27] << 8 | Reg[28]
          v = v / 4095.0 * 3.0
          app.var_value1.set("%.3f volts" % v)
        elif name == "ADC2":
          read_i2c = self.ReadBoard(29)
          if read_i2c:
            Reg[29:29 + 4] = read_i2c
          v = Reg[29] << 8 | Reg[30]
          v = v / 4095.0 * 3.0
          app.var_value1.set("%.3f volts" % v)
      if self.poll_state >= 5:
        self.poll_state = 0
      else:
        self.poll_state += 1

def FreqFormatter(freq):	# Format the string or integer frequency by adding blanks
  freq = int(freq)
  if freq >= 0:
    t = str(freq)
    minus = ''
  else:
    t = str(-freq)
    minus = '- '
  l = len(t)
  if l > 9:
    txt = "%s%s %s %s %s" % (minus, t[0:-9], t[-9:-6], t[-6:-3], t[-3:])
  elif l > 6:
    txt = "%s%s %s %s" % (minus, t[0:-6], t[-6:-3], t[-3:])
  elif l > 3:
    txt = "%s%s %s" % (minus, t[0:-3], t[-3:])
  else:
    txt = minus + t
  return txt

class ChangeDialog(tk.Toplevel):
  def __init__(self, title):
    tk.Toplevel.__init__(self)
    self.ok = False
    self.value = None
    self.title(title)
    self.resizable(width=False, height=False)
    self.rowconfigure(0, weight=1)
    self.var = tk.StringVar()
    en = ttk.Entry(self, exportselection=0, textvariable=self.var)
    en.grid(column=0, row=0, columnspan=2, padx=2)
    ttk.Button(self, text="OK", command=self.OnOk).grid(column=0, row=1, padx=2)
    ttk.Button(self, text="Cancel", command=self.OnCancel).grid(column=1, row=1, padx=2)
    self.bind('<Return>', self.OnOk)
    self.bind('<Escape>', self.OnCancel)
    self.transient(app)
    self.wait_visibility()
    self.grab_set()
    en.focus_set()
    # set the location (x, y) but not the size
    wxh, x1, y1 = app.geometry().split('+')
    wxh, x, y = self.geometry().split('+')
    x = int(x1) + 50
    y = int(y1) + 50
    self.geometry("%s+%d+%d" % (wxh, x, y))
    self.wait_window()
  def OnOk(self, event=None):
    self.ok = True
    self.value = self.var.get()
    self.grab_release()
    self.destroy()
  def OnCancel(self, event=None):
    self.ok = False
    self.grab_release()
    self.destroy()

class Application(tk.Tk):
  name2index = {	# GPIO name to its register number
    "Out1":16, "Out2":19, "Out3":20, "Out4":11, "Out5":10, "Out6":22, "Out7":9, "Out8":8, "Sw5":12, "Sw12":1}
  def __init__(self):
    "Make a top-level window, a scrollable canvas and two frames within the canvas to hold the controls."
    tk.Tk.__init__(self)
    self.minsize(100, 50)
    self.known_ip = ''
    self.title("N2ADR HL2 IO Board Control " + VERSION)
    s=ttk.Style()
    #print ("Ttk themes", s.theme_names())
    s.theme_use('clam')
    #s.theme_use('alt')
    #print ("Using theme", s.theme_use())
    canvas = tk.Canvas(self, highlightthickness=0)
    yscroll = ttk.Scrollbar(self, orient=tk.VERTICAL, command=canvas.yview)
    xscroll = ttk.Scrollbar(self, orient=tk.HORIZONTAL, command=canvas.xview)
    canvas.config(xscrollcommand=xscroll.set, yscrollcommand=yscroll.set)
    canvas.grid(column=0, row=0, sticky=tk.N+tk.W+tk.E+tk.S)
    yscroll.grid(column=1, row=0, sticky=tk.N+tk.S)
    xscroll.grid(column=0, row=1, sticky=tk.W+tk.E)
    self.columnconfigure(0, weight=1)
    self.rowconfigure(0, weight=1)
    # Make the top frame
    self.topframe = ttk.Frame(canvas)
    self.MakeTopframe()
    self.topframe.update_idletasks()
    x, y, topW, topH = self.topframe.grid_bbox()
    # Make the mainframe
    self.mainframe = ttk.Frame(canvas)
    row = col = 0
    col, row = self.MakeGpioRow(col, row, self.mainframe)
    col, row = self.MakeMacroRow(col, row, self.mainframe, "MacroA")
    col, row = self.MakeMacroRow(col, row, self.mainframe, "MacroB")
    self.mainframe.update_idletasks()
    x, y, mainW, mainH = self.mainframe.grid_bbox()
    # Add the frames to the canvas
    width = max(topW, mainW) + 10
    height = topH + mainH + 10
    canvas.create_window(0, 0, anchor=tk.NW, window=self.topframe)
    canvas.create_window(0, topH, anchor=tk.NW, window=self.mainframe)
    canvas.config(scrollregion=(0, 0, width, height), width=width, height=height)
    self.protocol("WM_DELETE_WINDOW", self.OnExit)
    self.comm_thread = CommThread(self)
    self.comm_thread.daemon = True
    self.comm_thread.start()
  def OnExit(self):
    app.comm_thread.stop()
    time.sleep(1.0)
    #for i in range(20):
    #while self.comm_thread.is_alive():
    #  print (self.comm_thread.is_alive())
    #  time.sleep(0.1)
    self.destroy()
  def ChangeGpio(self, name):
    value = getattr(self, name).get()
    index = self.name2index[name]
    self.comm_thread.WriteBoard(170 + index, value)
  def ChangeRegister(self, event, title=None, index=None):
    if index is None:
      index = app.reg_index.get()
      try:
        index = int(index)
      except:
        return
    if title is None:
      title = "Change Register %d" % index
    dlg = ChangeDialog(title)
    if dlg.ok:
      text = dlg.value
    else:
      return
    try:
      value = int(text, base=0)
    except:
      return
    self.comm_thread.WriteBoard(index, value)
  def ChangePinGpio(self, event):
    index = app.gpio_index.get()
    try:
      index = int(index)
    except:
      return
    title = "Change GPIO %d" % index
    self.ChangeRegister(event, title=title, index=index+170)
  def ChangeValue1(self, event):
    name = self.var_name1.get()
    dlg = ChangeDialog(name)
    if dlg.ok:
      text = dlg.value
    else:
      return
    if name == "Band Volts":
      try:
        volts = float(text)
        if volts < 0:
          volts = 0
        data = int(volts / 5.0 * 255.0 + 0.5)
        if data > 255:
          data = 255
      except:
        return
      self.comm_thread.WriteBoard(178, data)
    elif name == "Fan Volts":
      try:
        volts = float(text)
        if volts < 0:
          volts = 0
        data = int(255.0 * (volts + 0.7) / 3.3 / 3.7 + 0.5)
        if data > 255:
          data = 255
      except:
        return
      self.comm_thread.WriteBoard(12, data)
  def KnownIP(self, event):
    t = self.hl2_ip.get()
    if t.count('.') == 3:
      self.known_ip = t
    else:
      self.known_ip = ''
  def MakeTopframe(self):
    frame = self.topframe
    s = ttk.Style()
    s.configure('Box.TLabel', background='#EEE') #, borderwidth=0, relief=tk.SUNKEN)
    #s.configure('Readonly.TCombobox', fieldbackground='red') #, borderwidth=0, relief=tk.SUNKEN)
    s.map('Readonly.TCombobox', fieldbackground=[('readonly','white')])
    s.map('Readonly.TCombobox', selectbackground=[('readonly', 'white')])
    s.map('Readonly.TCombobox', selectforeground=[('readonly', 'black')])
    s.map('Readonly.TCombobox', background=[('readonly', 'white')])
    # First three columns
    col = 0
    row = 0
    c = ttk.Label(frame, text="Known IP or blank")
    c.grid(column=col, row=row, columnspan=2, sticky=tk.W, padx=(10, 5))
    self.hl2_ip = tk.StringVar()
    en = ttk.Entry(frame, exportselection=0, width=15, textvariable=self.hl2_ip)
    en.grid(column=col + 2, row=row, sticky=(tk.W, tk.E), padx=5, pady=2)
    en.bind('<Return>', self.KnownIP)
    row += 1
    bv = ttk.Label(frame, text="Register")
    bv.grid(column=col, row=row, sticky=tk.W, padx=(10, 5))
    self.reg_index = tk.StringVar()
    self.reg_index.set("4")
    en = ttk.Entry(frame, exportselection=0, width=5, textvariable=self.reg_index)
    en.grid(column=col + 1, row=row, sticky=tk.E, padx=5, pady=2)
    self.reg_value = tk.StringVar()
    bv = ttk.Label(frame, textvariable=self.reg_value, style='Box.TLabel', background='#FFF')
    bv.grid(column=col + 2, row=row, sticky=(tk.W, tk.E), padx=(10, 5), pady=2)
    bv.bind('<ButtonRelease>', self.ChangeRegister)
    c = ttk.Label(frame, text=" ")	# Add a spacer
    c.grid(column=col, row=row, sticky=tk.W, padx=5)
    row += 1
    bv = ttk.Label(frame, text="GPIO pin")
    bv.grid(column=col, row=row, sticky=tk.W, padx=(10, 5))
    self.gpio_index = tk.StringVar()
    self.gpio_index.set("8")
    en = ttk.Entry(frame, exportselection=0, width=5, textvariable=self.gpio_index)
    en.grid(column=col + 1, row=row, sticky=tk.E, padx=5, pady=2)
    self.gpio_value = tk.StringVar()
    bv = ttk.Label(frame, textvariable=self.gpio_value, style='Box.TLabel', background='#FFF')
    bv.grid(column=col + 2, row=row, sticky=(tk.W, tk.E), padx=5, pady=2)
    bv.bind('<ButtonRelease>', self.ChangePinGpio)
    c = ttk.Label(frame, text=" ")	# Add a spacer
    c.grid(column=col, row=row, sticky=tk.W, padx=5)
    row += 1
    # Combo control
    self.var_name1 = tk.StringVar()
    c = ttk.Combobox(frame, exportselection=0, width=8, state="readonly", style="Readonly.TCombobox", textvariable=self.var_name1,
        values=("Band Volts", "Fan Volts", "ADC0", "ADC1", "ADC2"))
    c.grid(column=col, row=row, columnspan=2, sticky=(tk.W, tk.E), padx=(10, 5))
    c.current(0)
    self.var_value1 = tk.StringVar()
    bv = ttk.Label(frame, textvariable=self.var_value1, style='Box.TLabel', background='#FFF')
    bv.grid(column=col + 2, row=row, sticky=(tk.W, tk.E), padx=5, pady=2)
    bv.bind('<ButtonRelease>', self.ChangeValue1)
    row += 1
    c = ttk.Label(frame, text=" ")
    c.grid(column=col, row=row, padx=5)
    # Second two columns
    row = 0
    col = 3
    c = ttk.Label(frame, text="Connected IP")
    c.grid(column=col, row=row, sticky=tk.W, padx=5)
    self.IP = tk.StringVar()
    c = ttk.Label(frame, textvariable=self.IP, width=22, style='Box.TLabel')
    c.grid(column=col + 1, row=row, sticky=tk.W, padx=5)
    row += 1
    c = ttk.Label(frame, text="Tx frequency")
    c.grid(column=col, row=row, sticky=tk.W, padx=5)
    self.tx_freq = tk.StringVar()
    c = ttk.Label(frame, textvariable=self.tx_freq, width=22, style='Box.TLabel')
    c.grid(column=col + 1, row=row, sticky=tk.W, padx=5)
    row += 1
    c = ttk.Label(frame, text="Temperature")
    c.grid(column=col, row=row, sticky=tk.W, padx=5)
    self.temp = tk.StringVar()
    c = ttk.Label(frame, textvariable=self.temp, width=22, style='Box.TLabel')
    c.grid(column=col + 1, row=row, sticky=tk.W, padx=5)
  def MakeGpioRow(self, col, row, mainframe):
    c = ttk.Label(mainframe, text="GPIO")
    c.grid(column=col, row=row + 1)
    col += 1
    for i in range(1, 9):
      v = tk.IntVar()
      t = "Out%d" % i
      setattr(self, "Out%d" % i, v)
      ttk.Label(mainframe, text=t).grid(column=col, row=row)
      cb = ttk.Checkbutton(mainframe, text='', command=partial(self.ChangeGpio, t), variable=v)
      cb.grid(column=col, row=row + 1)
      if i == 1:
        self.ctrlOut1 = cb
      elif i == 8:
        self.ctrlOut8 = cb
      col += 1
    self.Sw5 = tk.IntVar()
    self.Sw12 = tk.IntVar()
    c = ttk.Label(mainframe, text="Sw5")
    c.grid(column=col, row=row)
    c = ttk.Checkbutton(mainframe, text='', command=partial(self.ChangeGpio, "Sw5"), variable=self.Sw5)
    c.grid(column=col, row=row + 1)
    col += 1
    c = ttk.Label(mainframe, text="Sw12")
    c.grid(column=col, row=row)
    c = ttk.Checkbutton(mainframe, text='', command=partial(self.ChangeGpio, "Sw12"), variable=self.Sw12)
    c.grid(column=col, row=row + 1)
    col += 1
    for i in range(1, 6):
      v = tk.IntVar()
      t = "In%d" % i
      setattr(self, t, v)
      c = ttk.Label(mainframe, text=t)
      c.grid(column=col, row=row)
      cb = ttk.Checkbutton(mainframe, text='', state='disable', variable=v)
      cb.grid(column=col, row=row + 1)
      if i == 1:
        self.ctrlIn1 = cb
      col += 1
    self.EXTTR = tk.IntVar()
    c = ttk.Label(mainframe, text="Rx")
    c.grid(column=col, row=row)
    c = ttk.Checkbutton(mainframe, text='', state='disable', variable=self.EXTTR)
    c.grid(column=col, row=row + 1)
    row += 2
    col = 0
    return col, row
  def MakeMacroRow(self, col, row, mainframe, label):
    c = ttk.Button(self.mainframe, text=label, command=partial(self.OnMacro, label))
    c.grid(column=col, row=row, sticky=tk.W, padx=5, pady=2)
    col += 1
    for i in range(1, 9):
      v = tk.IntVar()
      setattr(self, "%sOut%d" % (label, i), v)
      c = ttk.Checkbutton(mainframe, text='', variable=v)
      c.grid(column=col, row=row, padx=5)
      col += 1
    v = tk.IntVar()
    setattr(self, "%sSw5" % label, v)
    c = ttk.Checkbutton(mainframe, text='', variable=v)
    c.grid(column=col, row=row, padx=5)
    col += 1
    v = tk.IntVar()
    setattr(self, "%sSw12" % label, v)
    c = ttk.Checkbutton(mainframe, text='', variable=v)
    c.grid(column=col, row=row, padx=5)
    row += 1
    col = 0
    return col, row
  def OnMacro(self, label):
    data = 0
    for i in range(1, 9):
      value = getattr(self, "%sOut%d" % (label, i)).get()
      if value == 1:
        data |= 1 << (i - 1)
    self.comm_thread.WriteBoard(169, data)
    v = getattr(self, "%sSw5" % label).get()
    self.comm_thread.WriteBoard(170 + 12, v)
    v = getattr(self, "%sSw12" % label).get()
    self.comm_thread.WriteBoard(170 + 1, v)

if __name__ == "__main__":
  app = Application()
  app.mainloop()
  app.comm_thread.stop()
