/* decrypt.h - this file is part of DeSmuME
 *
 * Copyright (C) 2006 Rafael Vuijk
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _DECRYPT_H_
#define _DECRYPT_H_

extern const unsigned char arm7_key[];

//decrypts the secure area of a rom (or does nothing if it is already decrypted)
bool DecryptSecureArea(u8 *romheader, u8 *secure);

//encrypts the secure area of a rom (or does nothing if it is already encrypted)
bool EncryptSecureArea(u8 *romheader, u8 *secure);

//since we have rom-type detection heuristics here, this module is responsible for checking whether a rom is valid
bool CheckValidRom(u8 *header, u8 *secure);

#endif
