/*
** Copyright (C) 2004 Jesse Chappell <jesse@essej.net>
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**  
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**  
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**  
*/

#include "core_midi_bridge.hpp"
#include <pthread.h>


//#define MIDIDEBUG

#ifdef MIDIDEBUG
#define DBG(x) x
#else
#define DBG(x)
#endif

using namespace SooperLooper;
using namespace std;

CoreMidiBridge::CoreMidiBridge (string name, string oscurl)
	: MidiBridge (name, oscurl), _seq(0)
{
	_done = false;
	_midi_thread = 0;
	
	if ((_seq = create_sequencer (name, true)) == 0) {
		return;
	}
	
	
	pthread_create (&_midi_thread, NULL, &CoreMidiBridge::_midi_receiver, this);
	if (!_midi_thread) {
		return;
	}
	
	pthread_detach(_midi_thread);
}

CoreMidiBridge::~CoreMidiBridge()
{
	_done = true;
	// don't try too hard
}

snd_seq_t *
CoreMidiBridge::create_sequencer (string client_name, bool isinput)
{
	snd_seq_t * seq;
	int err;

	string portname (client_name);
	
	if ((err = snd_seq_open (&seq, "default", SND_SEQ_OPEN_DUPLEX, 0)) != 0) {
		fprintf (stderr, "Could not open CoreMIDI sequencer, aborting\n\n%s\n\n"
			   "Make sure you have configure CoreMIDI properly and that\n"
			   "/proc/asound/seq/clients exists and contains relevant\n"
			   "devices.", 
			   snd_strerror (err));
		return 0;
	}
	
	snd_seq_set_client_name (seq, client_name.c_str());

	if (isinput) {
		portname += "_Input";
	} else {
		portname += "_Output";
	}
	
	if ((err = snd_seq_create_simple_port (seq, portname.c_str(),
					       (isinput? SND_SEQ_PORT_CAP_WRITE: SND_SEQ_PORT_CAP_READ)| SND_SEQ_PORT_CAP_DUPLEX |
					       SND_SEQ_PORT_CAP_SUBS_READ|SND_SEQ_PORT_CAP_SUBS_WRITE,
					       SND_SEQ_PORT_TYPE_APPLICATION|SND_SEQ_PORT_TYPE_SPECIFIC)) != 0) {
		fprintf (stderr, "Could not create CoreMIDI port: %s", snd_strerror (err));
		snd_seq_close(seq);
		return 0;
	}
	
	return seq;
}
	

void * CoreMidiBridge::_midi_receiver(void *arg)
{
	CoreMidiBridge * bridge = static_cast<CoreMidiBridge*> (arg);

	bridge->midi_receiver();
	return 0;
}

void  CoreMidiBridge::midi_receiver()
{
	snd_seq_event_t *event;
	int val;

	while (!_done) {

		snd_seq_event_input (_seq, &event);

		if (_done) {
			break;
		}

		switch(event->type){
		case SND_SEQ_EVENT_NOTEON:
			queue_midi(0x90+event->data.note.channel,event->data.note.note,event->data.note.velocity);
			DBG (printf("Noteon, channel: %d note: %d vol: %d\n",event->data.note.channel,event->data.note.note,event->data.note.velocity);)
			break;
		case SND_SEQ_EVENT_NOTEOFF:
			queue_midi(0x90+event->data.note.channel,event->data.note.note,0);
			DBG(printf("Noteoff, channel: %d note: %d vol: %d\n",event->data.note.channel,event->data.note.note,event->data.note.velocity);)
			break;
		case SND_SEQ_EVENT_KEYPRESS:
			queue_midi(0xa0+event->data.note.channel,event->data.note.note,event->data.note.velocity);
			DBG(printf("Keypress, channel: %d note: %d vol: %d\n",event->data.note.channel,event->data.note.note,event->data.note.velocity);)
			break;
		case SND_SEQ_EVENT_CONTROLLER:
			queue_midi(0xb0+event->data.control.channel,event->data.control.param,event->data.control.value);
			DBG(printf("Control: %d %3d %3d\n",event->data.control.channel,event->data.control.param,event->data.control.value);)
			break;
		case SND_SEQ_EVENT_PITCHBEND:
			val=event->data.control.value + 0x2000;
			queue_midi(0xe0+event->data.control.channel,val&127,val>>7);
			DBG(printf("Pitch: %d %d %d\n",event->data.control.channel,event->data.control.param,event->data.control.value);)
			break;
		case SND_SEQ_EVENT_CHANPRESS:
			queue_midi(0xc0+event->data.control.channel,event->data.control.value,0);
			DBG(printf("chanpress: %d %d %d\n",event->data.control.channel,event->data.control.param,event->data.control.value);)
			break;
		case SND_SEQ_EVENT_PGMCHANGE:
			queue_midi(0xc0+event->data.control.channel,event->data.control.value,0);
			DBG(printf("pgmchange: %d %d %d\n",event->data.control.channel,event->data.control.param,event->data.control.value);)
			break;
		case SND_SEQ_EVENT_START:
			queue_midi(0xfa,0,0);
			break;
		case SND_SEQ_EVENT_STOP:
			queue_midi(0xfc,0,0);
			break;
		case SND_SEQ_EVENT_CLOCK:
			queue_midi(0xf8,0,0);
			break;
		default:
			//printf("Unknown type: %d\n",event->type);
			break;
		}
	}

	snd_seq_close (_seq);
	_seq = 0;
}

