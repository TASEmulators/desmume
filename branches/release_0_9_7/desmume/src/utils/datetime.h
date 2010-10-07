/*	Copyright (C) 2010 DeSmuME team

	This file is based on System.DateTime.cs and System.TimeSpan.cs from mono-2.6.7

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

//
// System.DateTime.cs
//
// author:
//   Marcel Narings (marcel@narings.nl)
//   Martin Baulig (martin@gnome.org)
//   Atsushi Enomoto (atsushi@ximian.com)
//
//   (C) 2001 Marcel Narings
// Copyright (C) 2004-2006 Novell, Inc (http://www.novell.com)
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
// 
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

//
// System.TimeSpan.cs
//
// Authors:
//   Duco Fijma (duco@lorentz.xs4all.nl)
//   Andreas Nahr (ClassDevelopment@A-SoftTech.com)
//   Sebastien Pouliot  <sebastien@ximian.com>
//
// (C) 2001 Duco Fijma
// (C) 2004 Andreas Nahr
// Copyright (C) 2004 Novell (http://www.novell.com)
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
// 
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//

#ifndef _DATETIME_H_
#define _DATETIME_H_

#include <math.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

#include <string>

#include "../types.h"

enum DayOfWeek {
	DayOfWeek_Sunday=0,
	DayOfWeek_Monday=1,
	DayOfWeek_Tuesday=2,
	DayOfWeek_Wednesday=3,
	DayOfWeek_Thursday=4,
	DayOfWeek_Friday=5,
	DayOfWeek_Saturday=6
};

class TimeSpan
{
	friend class DateTime;

public:
	static const TimeSpan& get_MaxValue()
	{
		static TimeSpan val(0x7FFFFFFFFFFFFFFFLL);
		return val;
	}

	static const TimeSpan& get_MinValue()
	{
		static TimeSpan val(0x8000000000000000LL);
		return val;
	}

	static const TimeSpan& get_Zero()
	{
		static TimeSpan val(0);
		return val;
	}

	static const s64 TicksPerDay = 864000000000LL;
	static const s64 TicksPerHour = 36000000000LL;
	static const s64 TicksPerMillisecond = 10000LL;
	static const s64 TicksPerMinute = 600000000LL;
	static const s64 TicksPerSecond = 10000000LL;

	TimeSpan ()
	{
	}

	TimeSpan (s64 ticks)
		: _ticks(ticks)
	{
	}

	TimeSpan (int hours, int minutes, int seconds)
	{
		_ticks = CalculateTicks (0, hours, minutes, seconds, 0);
	}

	TimeSpan (int days, int hours, int minutes, int seconds)
	{
		_ticks = CalculateTicks (days, hours, minutes, seconds, 0);
	}

	TimeSpan (int days, int hours, int minutes, int seconds, int milliseconds)
	{
		_ticks = CalculateTicks (days, hours, minutes, seconds, milliseconds);
	}

	int get_Days() const { return (int) (_ticks / TicksPerDay); }
	int get_Hours() const { return (int) (_ticks % TicksPerDay / TicksPerHour); }
	int get_Milliseconds() const { return (int) (_ticks % TicksPerSecond / TicksPerMillisecond); }
	int get_Minutes() const { return (int) (_ticks % TicksPerHour / TicksPerMinute); }
	int get_Seconds() const { return (int) (_ticks % TicksPerMinute / TicksPerSecond); }
	s64 get_Ticks() const { return _ticks; }
	double get_TotalDays() const { return (double) _ticks / TicksPerDay; }
	double get_TotalHours() const { return (double) _ticks / TicksPerHour; }
	double get_TotalMilliseconds() const { return (double) _ticks  / TicksPerMillisecond; }
	double get_TotalMinutes() const { return (double) _ticks / TicksPerMinute; }
	double get_TotalSeconds() const { return (double) _ticks / TicksPerSecond; }

	TimeSpan Add (const TimeSpan &ts)
	{
		return TimeSpan (_ticks + ts._ticks);
		//removed:
		//catch (OverflowException) throw new OverflowException (Locale.GetText ("Resulting timespan is too big."));
	}

	static int Compare (const TimeSpan& t1, const TimeSpan& t2)
	{
		if (t1._ticks < t2._ticks)
			return -1;
		if (t1._ticks > t2._ticks)
			return 1;
		return 0;
	}

	int CompareTo (const TimeSpan& value)
	{
		return Compare (*this, value);
	}

	TimeSpan Duration ()
	{
		return TimeSpan(_ticks<0?-_ticks:_ticks);
		//removed:
		//catch (OverflowException) throw new OverflowException (Locale.GetText ("This TimeSpan value is MinValue so you cannot get the duration."));
	}

	static TimeSpan FromDays (double value)
	{
		return From (value, TicksPerDay);
	}

	static TimeSpan FromHours (double value)
	{
		return From (value, TicksPerHour);
	}

	static TimeSpan FromMinutes (double value)
	{
		return From (value, TicksPerMinute);
	}

	static TimeSpan FromSeconds (double value)
	{
		return From (value, TicksPerSecond);
	}

	static TimeSpan FromMilliseconds (double value)
	{
		return From (value, TicksPerMillisecond);
	}

	static TimeSpan FromTicks (s64 value)
	{
		return TimeSpan (value);
	}

	TimeSpan Negate ()
	{
		//removed error handling
		//if (_ticks == MinValue()._ticks) throw new OverflowException (Locale.GetText ( "This TimeSpan value is MinValue and cannot be negated."));
		return TimeSpan (-_ticks);
	}

	TimeSpan Subtract (const TimeSpan& ts)
	{
		//removed error handling
		//try { checked { 
		return TimeSpan (_ticks - ts._ticks);
		//	}
		//}
		//catch (OverflowException) {
		//	throw new OverflowException (Locale.GetText ("Resulting timespan is too big."));
		//}
	}

	TimeSpan operator + (const TimeSpan& t2) const
	{
		TimeSpan temp = *this;
		temp.Add (t2);
		return temp;
	}

	bool operator == (const TimeSpan& t2) const 
	{
		return _ticks == t2._ticks;
	}

	bool operator > (const TimeSpan& t2) const 
	{
		return _ticks > t2._ticks;
	}

	bool operator >= (const TimeSpan& t2) const 
	{
		return _ticks >= t2._ticks;
	}

	bool operator != (const TimeSpan& t2) const 
	{
		return _ticks != t2._ticks;
	}

	bool operator < (const TimeSpan& t2) const 
	{
		return _ticks < t2._ticks;
	}

	bool operator <= (const TimeSpan& t2) const 
	{
		return _ticks <= t2._ticks;
	}

	TimeSpan operator - (const TimeSpan& t2) const 
	{
		TimeSpan temp = *this;
		return temp.Subtract (t2);
	}

	TimeSpan operator - ()
	{
		return Negate ();
	}


private:
	s64 _ticks;

	static s64 CalculateTicks (int days, int hours, int minutes, int seconds, int milliseconds)
	{
		// there's no overflow checks for hours, minutes, ...
		// so big hours/minutes values can overflow at some point and change expected values
		int hrssec = (hours * 3600); // break point at (Int32.MaxValue - 596523)
		int minsec = (minutes * 60);
		s64 t = ((s64)(hrssec + minsec + seconds) * 1000L + (s64)milliseconds);
		t *= 10000;

		bool overflow = false;
		// days is problematic because it can overflow but that overflow can be 
		// "legal" (i.e. temporary) (e.g. if other parameters are negative) or 
		// illegal (e.g. sign change).
		if (days > 0) {
			s64 td = TicksPerDay * days;
			if (t < 0) {
				s64 ticks = t;
				t += td;
				// positive days -> total ticks should be lower
				overflow = (ticks > t);
			}
			else {
				t += td;
				// positive + positive != negative result
				overflow = (t < 0);
			}
		}
		else if (days < 0) {
			s64 td = TicksPerDay * days;
			if (t <= 0) {
				t += td;
				// negative + negative != positive result
				overflow = (t > 0);
			}
			else {
				s64 ticks = t;
				t += td;
				// negative days -> total ticks should be lower
				overflow = (t > ticks);
			}
		}

		//removed:
		//if (overflow) throw ArgumentOutOfRangeException ("The timespan is too big or too small.");

		return t;
	}

	static TimeSpan From (double value, s64 tickMultiplicator) 
	{
		//a bunch of error handling removed

		//if (Double.IsNaN (value)) throw new ArgumentException (Locale.GetText ("Value cannot be NaN."), "value");
		//if (Double.IsNegativeInfinity (value) || Double.IsPositiveInfinity (value) ||
		//	(value < MinValue.Ticks) || (value > MaxValue.Ticks))
		//	throw new OverflowException (Locale.GetText ("Outside range [MinValue,MaxValue]"));

		//try {
		value = (value * (tickMultiplicator / TicksPerMillisecond));

		//	checked {
		//		long val = (long) Math.Round(value);
		//		return new TimeSpan (val * TicksPerMillisecond);
		//	}
		//}
		//catch (OverflowException) {
		//	throw new OverflowException (Locale.GetText ("Resulting timespan is too big."));
		//}
		//}
	}

};

class DateTime
{
private:
	TimeSpan ticks;

	static inline double round(const double x) { return floor(x + 0.5); }

	static const int dp400 = 146097;
	static const int dp100 = 36524;
	static const int dp4 = 1461;

	// w32 file time starts counting from 1/1/1601 00:00 GMT
	// which is the constant ticks from the .NET epoch
	static const s64 w32file_epoch = 504911232000000000LL;

	//private const long MAX_VALUE_TICKS = 3155378975400000000L;
	// -- Microsoft .NET has this value.
	static const s64 MAX_VALUE_TICKS = 3155378975999999999LL;

	//
	// The UnixEpoch, it begins on Jan 1, 1970 at 0:0:0, expressed
	// in Ticks
	//
	static const s64 UnixEpoch = 621355968000000000LL;


	static const int daysmonth[13];
	static const int daysmonthleap[13];
	static const char* monthnames[13];

	void init (int year, int month, int day, int hour, int minute, int second, int millisecond)
	{
		//removed error handling
		/*	if ( year < 1 || year > 9999 || 
		month < 1 || month >12  ||
		day < 1 || day > DaysInMonth(year, month) ||
		hour < 0 || hour > 23 ||
		minute < 0 || minute > 59 ||
		second < 0 || second > 59 ||
		millisecond < 0 || millisecond > 999)
		throw new ArgumentOutOfRangeException ("Parameters describe an " +
		"unrepresentable DateTime.");*/

		ticks = TimeSpan (AbsoluteDays(year,month,day), hour, minute, second, millisecond);
	}



	static int AbsoluteDays (int year, int month, int day)
	{
		const int* days;
		int temp = 0, m=1 ;

		days = (IsLeapYear(year) ? daysmonthleap  : daysmonth);

		while (m < month)
			temp += days[m++];
		return ((day-1) + temp + (365* (year-1)) + ((year-1)/4) - ((year-1)/100) + ((year-1)/400));
	}


	enum Which 
	{
		Which_Day,
		Which_DayYear,
		Which_Month,
		Which_Year
	};

	int FromTicks(Which what) const
	{
		int num400, num100, num4, numyears; 
		int M =1;

		const int* days = daysmonth;
		int totaldays = ticks.get_Days();

		num400 = (totaldays / dp400);
		totaldays -=  num400 * dp400;

		num100 = (totaldays / dp100);
		if (num100 == 4)   // leap
			num100 = 3;
		totaldays -= (num100 * dp100);

		num4 = totaldays / dp4;
		totaldays -= (num4 * dp4);

		numyears = totaldays / 365 ;

		if (numyears == 4)  //leap
			numyears =3 ;
		if (what == Which_Year )
			return num400*400 + num100*100 + num4*4 + numyears + 1;

		totaldays -= (numyears * 365) ;
		if (what == Which_DayYear )
			return totaldays + 1;

		if  ((numyears==3) && ((num100 == 3) || !(num4 == 24)) ) //31 dec leapyear
			days = daysmonthleap;

		while (totaldays >= days[M])
			totaldays -= days[M++];

		if (what == Which_Month )
			return M;

		return totaldays +1; 
	}

public:
	DateTime()
		: ticks(0)
	{
	}

	static const char* GetNameOfMonth(int month) { return monthnames[month]; }

	DateTime (s64 ticks)
	{
		this->ticks = TimeSpan (ticks);
		//removed error handling
		//if (ticks < get_MinValue().get_Ticks() || ticks > get_MaxValue().get_Ticks()) {
		//	string msg = Locale.GetText ("Value {0} is outside the valid range [{1},{2}].", 
		//		ticks, MinValue.Ticks, MaxValue.Ticks);
		//	throw new ArgumentOutOfRangeException ("ticks", msg);
		//}
	}

	static const DateTime& get_MaxValue() {
		static DateTime val(false, TimeSpan (MAX_VALUE_TICKS));
		return val;
	}

	static const DateTime& get_MinValue() {
		static DateTime val(false, TimeSpan (0));
		return val;
	}

	DateTime (int year, int month, int day)
	{
		init(year,month,day,0,0,0,0);
	}

	DateTime (int year, int month, int day, int hour, int minute, int second)
	{
		init(year, month, day, hour, minute, second, 0);
	}

	DateTime (int year, int month, int day, int hour, int minute, int second, int millisecond)
	{
		init(year,month,day,hour,minute,second,millisecond);
	}


	DateTime (bool check, const TimeSpan& value)
	{
		//removed error handling
		//if (check && (value.Ticks < MinValue.Ticks || value.Ticks > MaxValue.Ticks))
		//	throw new ArgumentOutOfRangeException ();

		ticks = value;
	}

	DateTime get_Date () const
	{
		return DateTime (get_Year(), get_Month(), get_Day());
	}

	int get_Month () const {
		return FromTicks(Which_Month); 
	}

	int get_Day() const
	{
		return FromTicks(Which_Day); 
	}

	DayOfWeek get_DayOfWeek () const
	{
		return ( (DayOfWeek) ((ticks.get_Days()+1) % 7) ); 
	}

	int get_DayOfYear () const
	{
		return FromTicks(Which_DayYear); 
	}

	TimeSpan get_TimeOfDay () const
	{
		return TimeSpan(ticks.get_Ticks() % TimeSpan::TicksPerDay );
	}

	int get_Hour () const
	{
		return ticks.get_Hours();
	}

	int get_Minute () const
	{
		return ticks.get_Minutes();
	}

	int get_Second () const
	{
		return ticks.get_Seconds();
	}

	int get_Millisecond () const
	{
		return ticks.get_Milliseconds();
	}

	//internal static extern s64 GetTimeMonotonic ();
	//internal static extern s64 GetNow ();

	static DateTime get_Now ()
	{
		time_t timer;
		time(&timer);
		struct tm *tm = localtime(&timer);
		return DateTime(tm->tm_year+1900,tm->tm_mon+1,tm->tm_mday,tm->tm_hour,tm->tm_min,tm->tm_sec);
	}

	s64 get_Ticks()const
	{ 
		return ticks.get_Ticks();
	}

	static DateTime get_Today ()
	{
		DateTime now = get_Now();
		DateTime today = DateTime (now.get_Year(), now.get_Month(), now.get_Day());
		return today;
	}

	int get_Year () const
	{
		return FromTicks(Which_Year); 
	}

	DateTime Add (const TimeSpan& value) const
	{
		DateTime ret = AddTicks (value.get_Ticks());
		return ret;
	}

	DateTime AddDays (double value) const
	{
		return AddMilliseconds (round(value * 86400000));
	}

	DateTime AddTicks (const s64 value) const
	{
		//removed error handling
		//if ((value + ticks.Ticks) > MAX_VALUE_TICKS || (value + ticks.Ticks) < 0) {
		//	throw new ArgumentOutOfRangeException();
		//}
		return DateTime (value + ticks.get_Ticks());
	}

	DateTime AddHours (double value) const
	{
		return AddMilliseconds (value * 3600000);
	}

	DateTime AddMilliseconds (double value) const
	{
		//removed error handling
		/*		if ((value * TimeSpan.TicksPerMillisecond) > long.MaxValue ||
		(value * TimeSpan.TicksPerMillisecond) < long.MinValue) {
		throw new ArgumentOutOfRangeException();
		}
		*/		
		s64 msticks = (s64) round(value * TimeSpan::TicksPerMillisecond);
		return AddTicks (msticks);
	}

	DateTime AddMinutes (double value) const
	{
		return AddMilliseconds (value * 60000);
	}

	DateTime AddMonths (int months) const
	{
		int day, month, year,  maxday ;
		DateTime temp;

		day = get_Day();
		month = get_Month() + (months % 12);
		year = get_Year() + months/12 ;

		if (month < 1)
		{
			month = 12 + month ;
			year -- ;
		}
		else if (month>12) 
		{
			month = month -12;
			year ++;
		}
		maxday = DaysInMonth(year, month);
		if (day > maxday)
			day = maxday;

		temp = (year, month, day);
		return  temp.Add (get_TimeOfDay());
	}

	DateTime AddSeconds (double value) const
	{
		return AddMilliseconds (value * 1000);
	}

	DateTime AddYears (int value) const
	{
		return AddMonths (value * 12);
	}

	static int Compare (const DateTime& t1,	const DateTime& t2)
	{
		if (t1.ticks < t2.ticks) 
			return -1;
		else if (t1.ticks > t2.ticks) 
			return 1;
		else
			return 0;
	}

	static int DaysInMonth (int year, int month)
	{
		const int* days ;

		//removed error handling
		//if (month < 1 || month >12)throw new ArgumentOutOfRangeException ();
		//if (year < 1 || year > 9999)throw new ArgumentOutOfRangeException ();

		days = (IsLeapYear(year) ? daysmonthleap  : daysmonth);
		return days[month];			
	}
	static bool IsLeapYear (int year)
	{
		//removed error handling
		/*		if (year < 1 || year > 9999)
		throw new ArgumentOutOfRangeException ();*/
		return  ( (year % 4 == 0 && year % 100 != 0) || year % 400 == 0) ;
	}

	TimeSpan Subtract (const DateTime& value) const
	{
		return TimeSpan (ticks.get_Ticks()) - value.ticks;
	}

	DateTime Subtract(const TimeSpan& value) const
	{
		TimeSpan newticks;

		newticks = (TimeSpan (ticks.get_Ticks())) - value;
		DateTime ret = DateTime (true,newticks);
		return ret;
	}

	DateTime operator +(const TimeSpan& t) const
	{
		return DateTime (true, ticks + t);
	}

	bool operator ==(const DateTime& d2) const
	{
		return (ticks == d2.ticks);
	}

	bool operator >(const DateTime& t2) const
	{
		return (ticks > t2.ticks);
	}

	bool operator >=(const DateTime &t2) const
	{
		return (ticks >= t2.ticks);
	}

	bool operator !=(const DateTime& d2) const
	{
		return (ticks != d2.ticks);
	}

	bool operator <(const DateTime& t2) const
	{
		return (ticks < t2.ticks );
	}

	bool operator <=(const DateTime& t2) const
	{
		return (ticks <= t2.ticks);
	}

	TimeSpan operator -(const DateTime& d2) const
	{
		return TimeSpan((ticks - d2.ticks).get_Ticks());
	}

	DateTime operator -(const TimeSpan& t) const
	{
		return DateTime (true, ticks - t);
	}

	//try to have a canonical format here. this was comment was typed at 2010-oct-04 02:16:44:000

	std::string ToString() const
	{
		char tmp[32];
		sprintf(tmp,"%04d-%s-%02d %02d:%02d:%02d:%03d",get_Year(),monthnames[get_Month()],get_Day(),get_Hour(),get_Minute(),get_Second(),get_Millisecond());
		return tmp;
	}

	static bool TryParse(const char* str, DateTime& out)
	{
		int year,mon=-1,day,hour,min,sec,ms;
		char strmon[4];
		int done = sscanf(str,"%04d-%3s-%02d %02d:%02d:%02d:%03d",&year,strmon,&day,&hour,&min,&sec,&ms);
		if(done != 7) return false;
		for(int i=1;i<12;i++)
			if(!strncasecmp(monthnames[i],strmon,3))
			{
				mon=i;
				break;
			}
		if(mon==-1) return false;
		out = DateTime(year,mon,day,hour,min,sec);
		return true;
	}

	static DateTime Parse(const char* str)
	{
		DateTime ret;
		TryParse(str,ret);
		return ret;
	}

};


#endif //_DATETIME_H_
