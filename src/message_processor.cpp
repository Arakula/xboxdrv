/*
**  Xbox360 USB Gamepad Userspace Driver
**  Copyright (C) 2011 Ingo Ruhnke <grumbel@gmx.de>
**
**  This program is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "message_processor.hpp"

#include "log.hpp"
#include "uinput.hpp"

MessageProcessor::MessageProcessor(ControllerSlotConfigPtr config, const Options& opts) :
  m_config(config),
  m_oldmsg(),
  m_config_toggle_button(opts.config_toggle_button),
  m_rumble_gain(opts.rumble_gain),
  m_rumble_test(opts.rumble),
  m_rumble_callback()
{
}

MessageProcessor::~MessageProcessor()
{
}

void
MessageProcessor::send(const ControllerMessage& msg_in, 
                       const ControllerMessageDescriptor& msg_desc, 
                       int msec_delta)
{
  if (m_config && !m_config->empty())
  {
    ControllerMessage msg = msg_in; 

    if (m_rumble_test)
    {
      log_debug("rumble: " << msg.get_abs(XBOX_AXIS_LT) << " " << msg.get_abs(XBOX_AXIS_RT));

      set_rumble(static_cast<uint8_t>(msg.get_abs(XBOX_AXIS_LT)), 
                 static_cast<uint8_t>(msg.get_abs(XBOX_AXIS_RT)));
    }

    // handling switching of configurations
    if (m_config_toggle_button != XBOX_BTN_UNKNOWN)
    {
      bool last = m_oldmsg.get_key(m_config_toggle_button);
      bool cur  = msg.get_key(m_config_toggle_button);

      if (cur && cur != last)
      {
        // reset old mapping to zero to not get stuck keys/axis
        m_config->get_config()->get_uinput().reset_all_outputs();

        // switch to the next input mapping
        m_config->next_config();

        log_info("switched to config: " << m_config->get_current_config());
      }
    }

    // run the controller message through all modifier
    for(std::vector<ModifierPtr>::iterator i = m_config->get_config()->get_modifier().begin();
        i != m_config->get_config()->get_modifier().end(); 
        ++i)
    {
      (*i)->update(msec_delta, msg);
    }

    m_config->get_config()->get_uinput().update(msec_delta);

    // send current Xbox state to uinput
    if (memcmp(&msg, &m_oldmsg, sizeof(ControllerMessage)) != 0)
    {
      // Only send a new event out if something has changed,
      // this is useful since some controllers send events
      // even if nothing has changed, deadzone can cause this
      // too
      m_oldmsg = msg;

      m_config->get_config()->get_uinput().send(msg);
    }
  }
}

void
MessageProcessor::set_rumble(uint8_t lhs, uint8_t rhs)
{
  if (m_rumble_callback)
  {
    lhs = static_cast<uint8_t>(std::min(lhs * m_rumble_gain / 255, 255));
    rhs = static_cast<uint8_t>(std::min(rhs * m_rumble_gain / 255, 255));
  
    m_rumble_callback(lhs, rhs);
  }
}

void
MessageProcessor::set_config(int num)
{
  if (m_config)
  {
  m_config->set_current_config(num);
  }
}

void
MessageProcessor::set_ff_callback(const boost::function<void (uint8_t, uint8_t)>& callback)
{
  m_rumble_callback = callback;
  if (m_config)
  {
    m_config->set_ff_callback(callback);
  }
}

/* EOF */
