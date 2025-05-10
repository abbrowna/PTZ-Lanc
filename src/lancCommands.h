#ifndef LANC_COMMANDS_H
#define LANC_COMMANDS_H

#include <Arduino.h>

struct LancCommand {
  uint8_t byte0;
  uint8_t byte1;
};

const LancCommand ZOOM_IN[] = {{0x28, 0x00}, {0x28, 0x02}, {0x28, 0x04}, {0x28, 0x06},
                               {0x28, 0x08}, {0x28, 0x0A}, {0x28, 0x0C}, {0x28, 0x0E}};
const LancCommand ZOOM_OUT[] = {{0x28, 0x10}, {0x28, 0x12}, {0x28, 0x14}, {0x28, 0x16},
                                {0x28, 0x18}, {0x28, 0x1A}, {0x28, 0x1C}, {0x28, 0x1E}};

/*
const LancCommand ZOOM_IN[] = {{0x1E, 0x01}, {0x1E, 0x03}, {0x1E, 0x05}, {0x1E, 0x07},
                                {0x1E, 0x09}, {0x1E, 0x0B}, {0x1E, 0x0D}, {0x1E, 0x0F}};
const LancCommand ZOOM_OUT[] = {{0x1E, 0x11}, {0x1E, 0x13}, {0x1E, 0x15}, {0x1E, 0x17},
                                 {0x1E, 0x19}, {0x1E, 0x1B}, {0x1E, 0x1D}, {0x1E, 0x1F}};
*/
const LancCommand FOCUS_NEAR = {0x28, 0x47};
const LancCommand FOCUS_FAR = {0x28, 0x45};
const LancCommand UP = {0x18, 0x84};
const LancCommand DOWN = {0x18, 0x86};
const LancCommand SELECT = {0x18, 0xA2};

const LancCommand WB_DEC_K[] = {SELECT, DOWN, DOWN, DOWN, SELECT, SELECT, DOWN, DOWN, SELECT, UP, SELECT};
const LancCommand WB_INC_K[] = {SELECT, DOWN, DOWN, DOWN, SELECT, SELECT, DOWN, DOWN, DOWN, SELECT, UP, UP, SELECT};
const LancCommand WB_init[] = {SELECT, DOWN, DOWN, DOWN, SELECT, SELECT, DOWN, DOWN, SELECT,
                               SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT,
                               SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT,
                               SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT,
                               SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT,
                               SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT,
                               SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT,
                               SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT,
                               SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT,
                               SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT,
                               SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT,
                               SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT,
                               SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT,
                               SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT,
                               UP, SELECT};
const LancCommand WB_default[] = {SELECT, DOWN, DOWN, DOWN, SELECT, SELECT, DOWN, DOWN, DOWN,
                                  SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT,
                                  SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT,
                                  SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT,
                                  SELECT, SELECT, SELECT, SELECT,
                                  UP, UP, SELECT};
const LancCommand Exp_F_INC[] = {SELECT, DOWN, DOWN, SELECT, SELECT, DOWN, DOWN, DOWN, DOWN, DOWN, DOWN, SELECT, DOWN, DOWN, SELECT};
const LancCommand Exp_F_DEC[] = {SELECT, DOWN, DOWN, SELECT, SELECT, DOWN, DOWN, DOWN, SELECT, UP, UP, UP, UP, SELECT};
const LancCommand Exp_F_init[] = {SELECT, DOWN, DOWN, SELECT, SELECT, DOWN, DOWN, DOWN,
                                  SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT,
                                  SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT,
                                  SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT,
                                  UP, UP, UP, UP, SELECT};
const LancCommand Exp_F_default[] = {SELECT, DOWN, DOWN, SELECT, SELECT, DOWN, DOWN, DOWN, DOWN, DOWN, DOWN, 
                                     SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT,
                                     DOWN, DOWN, SELECT};
const LancCommand Exp_S_INC[] = {SELECT, DOWN, DOWN, SELECT, SELECT, DOWN, SELECT, DOWN, DOWN, DOWN, DOWN, DOWN, SELECT, UP, UP, UP, UP, UP, UP, SELECT, UP, SELECT};
const LancCommand Exp_S_DEC[] = {SELECT, DOWN, DOWN, SELECT, SELECT, DOWN, SELECT, DOWN, DOWN, SELECT, UP, UP, UP, SELECT, UP, SELECT};
const LancCommand Exp_S_init[] = {SELECT, DOWN, DOWN, SELECT, SELECT, DOWN, SELECT, DOWN, DOWN,
                                  SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT,
                                  UP, UP, UP, SELECT, UP, SELECT};
const LancCommand Exp_S_default[] = {SELECT, DOWN, DOWN, SELECT, SELECT, DOWN, SELECT, DOWN, DOWN, DOWN, DOWN, DOWN, 
                                     SELECT, SELECT, SELECT, 
                                     UP, UP, UP, UP, UP, UP, SELECT, UP, SELECT};
const LancCommand Exp_GAIN_INC[] = {SELECT, DOWN, DOWN, SELECT, SELECT, DOWN, DOWN, SELECT, DOWN, DOWN, DOWN, DOWN, SELECT, UP, UP, UP, UP, UP, UP, SELECT, UP, SELECT};
const LancCommand Exp_GAIN_DEC[] = {SELECT, DOWN, DOWN, SELECT, SELECT, DOWN, DOWN, SELECT, DOWN, SELECT, UP, UP, UP, SELECT, UP, SELECT};
const LancCommand Exp_GAIN_init[] = {SELECT, DOWN, DOWN, SELECT, SELECT, DOWN, DOWN, SELECT, DOWN,
                                     SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT,
                                     SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT,
                                     SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, 
                                     UP, UP, UP, SELECT, UP, SELECT};
const LancCommand Exp_GAIN_default[] = {SELECT, DOWN, DOWN, SELECT, SELECT, DOWN, DOWN, SELECT, DOWN, DOWN, DOWN, DOWN, 
                                        SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, 
                                        SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, SELECT, 
                                        UP, UP, UP, UP, UP, UP, SELECT, UP, SELECT};


#endif // LANC_COMMANDS_H