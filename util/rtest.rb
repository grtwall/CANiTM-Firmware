#!/usr/bin/ruby
# Ruby testing
require 'usb'
require 'pp'

canitm = nil

#pp USB.devices
USB.devices.each do |dev|
  canitm = dev if dev.idVendor.to_s(16) == "483" and dev.idProduct.to_s(16) == "f00f"
end

if canitm then
  puts "Found CAN in the Middle controler"
  canitm.open do |can|
    can.usb_claim_interface(0)
    puts "Enter data to send to USB, q to quit"
    quit = false
    while(!quit) do
      data = gets.chomp
      if not data == "q" then
        can.usb_bulk_write(2, data, 3000)
      else
        quit = true
      end
    end
  end
else
  puts "Could not locate CAN in the Middle device"
end
