#include "stdafx.h"
#include "sun.h"
#include "Globals.h"
#include "mtable.h"
#include "World.h"
#include "utilities.h"

//////////////////////////////////////////////////////////////////////////////////////////
// cSun -- class responsible for dynamic calculation of position and intensity of the Sun,

cSun::cSun() {

	setLocation( 19.00f, 52.00f );					// default location roughly in centre of Poland
	m_observer.press = 1013.0;						// surface pressure, millibars
	m_observer.temp = 15.0;							// ambient dry-bulb temperature, degrees C
}

cSun::~cSun() { gluDeleteQuadric( sunsphere ); }

void
cSun::init() {

    sunsphere = gluNewQuadric();
    gluQuadricNormals( sunsphere, GLU_SMOOTH );

#ifdef _WIN32
	TIME_ZONE_INFORMATION timezoneinfo;				// TODO: timezone dependant on geographic location
	::GetTimeZoneInformation( &timezoneinfo );
	m_observer.timezone = -timezoneinfo.Bias / 60.0f;
#elif __linux__
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    time_t local = mktime(localtime(&ts.tv_sec));
    time_t utc = mktime(gmtime(&ts.tv_sec));
	m_observer.timezone = (local - utc) / 3600.0f;
#endif
}

void
cSun::update() {

    move();
    glm::vec3 position( 0.f, 0.f, -1.f );
    position = glm::rotateX( position, glm::radians( static_cast<float>( m_body.elevref ) ) );
    position = glm::rotateY( position, glm::radians( static_cast<float>( -m_body.hrang ) ) );

    m_position = glm::normalize( position );
}

void
cSun::render() {

    ::glColor4f( 255.f / 255.f, 242.f / 255.f, 231.f / 255.f, 1.f );
	// debug line to locate the sun easier
    auto const position { m_position * 2000.f };
    ::glBegin( GL_LINES );
	::glVertex3fv( glm::value_ptr( position ) );
	::glVertex3f( position.x, 0.f, position.z );
	::glEnd();
	::glPushMatrix();
	::glTranslatef( position.x, position.y, position.z );
	// radius is a result of scaling true distance down to 2km -- it's scaled by equal ratio
	::gluSphere( sunsphere, m_body.distance * 9.359157, 12, 12 );
	::glPopMatrix();
}
/*
glm::vec3
cSun::getPosition() {
    
    return m_position * 1000.f * Global.fDistanceFactor;
}
*/
glm::vec3
cSun::getDirection() {

	return m_position;
}

float
cSun::getAngle() {
    
    return (float)m_body.elevref;
}

// return current hour angle
double
cSun::getHourAngle() const {

    return m_body.hrang;
}

float cSun::getIntensity() {

	irradiance();
	return (float)( m_body.etr/ 1399.0 );	// arbitrary scaling factor taken from etrn value
}

void cSun::setLocation( float const Longitude, float const Latitude ) {

	// convert fraction from geographical base of 6o minutes
	m_observer.longitude = (int)Longitude + (Longitude - (int)(Longitude)) * 100.0 / 60.0;
	m_observer.latitude = (int)Latitude + (Latitude - (int)(Latitude)) * 100.0 / 60.0 ;
}

// sets current time, overriding one acquired from the system clock
void cSun::setTime( int const Hour, int const Minute, int const Second ) {

    m_observer.hour = clamp( Hour, -1, 23 );
    m_observer.minute = clamp( Minute, -1, 59 );
    m_observer.second = clamp( Second, -1, 59 );
}

void cSun::setTemperature( float const Temperature ) {
    
    m_observer.temp = Temperature;
}

void cSun::setPressure( float const Pressure ) {
    
    m_observer.press = Pressure;
}

void cSun::move() {

    static double radtodeg = 57.295779513; // converts from radians to degrees
    static double degtorad = 0.0174532925; // converts from degrees to radians

    SYSTEMTIME localtime = simulation::Time.data(); // time for the calculation

    if( m_observer.hour >= 0 ) { localtime.wHour = m_observer.hour; }
    if( m_observer.minute >= 0 ) { localtime.wMinute = m_observer.minute; }
    if( m_observer.second >= 0 ) { localtime.wSecond = m_observer.second; }

    double ut =
        localtime.wHour
        + localtime.wMinute / 60.0 // too low resolution, noticeable skips
        + localtime.wSecond / 3600.0; // good enough in normal circumstances
/*
        + localtime.wMilliseconds / 3600000.0; // for really smooth movement
*/ 
   double daynumber =
       367 * localtime.wYear
        - 7 * ( localtime.wYear + ( localtime.wMonth + 9 ) / 12 ) / 4
        + 275 * localtime.wMonth / 9
        + localtime.wDay
        - 730530
        + ( ut / 24.0 );

    // Universal Coordinated (Greenwich standard) time
    m_observer.utime = ut * 3600.0;
    m_observer.utime = m_observer.utime / 3600.0 - m_observer.timezone;
    // perihelion longitude
    m_body.phlong = 282.9404 + 4.70935e-5 * daynumber; // w
    // orbit eccentricity
    double const e = 0.016709 - 1.151e-9 * daynumber;
    // mean anomaly
    m_body.mnanom = clamp_circular( 356.0470 + 0.9856002585 * daynumber ); // M
    // obliquity of the ecliptic
    m_body.oblecl = 23.4393 - 3.563e-7 * daynumber;
    // mean longitude
    m_body.mnlong = clamp_circular( m_body.phlong + m_body.mnanom ); // L = w + M
    // eccentric anomaly
    double const E = m_body.mnanom + radtodeg * e * std::sin( degtorad * m_body.mnanom ) * ( 1.0 + e * std::cos( degtorad * m_body.mnanom ) );
    // ecliptic plane rectangular coordinates
    double const xv = std::cos( degtorad * E ) - e;
    double const yv = std::sin( degtorad * E ) * std::sqrt( 1.0 - e*e );
    // distance
    m_body.distance = std::sqrt( xv*xv + yv*yv ); // r
    // true anomaly
    m_body.tranom = radtodeg * std::atan2( yv, xv ); // v
    // ecliptic longitude
    m_body.eclong = clamp_circular( m_body.tranom + m_body.phlong ); // lon = v + w
/*
    // ecliptic rectangular coordinates
    double const x = m_body.distance * std::cos( degtorad * m_body.eclong );
    double const y = m_body.distance * std::sin( degtorad * m_body.eclong );
    double const z = 0.0;
    // equatorial rectangular coordinates
    double const xequat = x;
    double const yequat = y * std::cos( degtorad * m_body.oblecl ) - 0.0 * std::sin( degtorad * m_body.oblecl );
    double const zequat = y * std::sin( degtorad * m_body.oblecl ) + 0.0 * std::cos( degtorad * m_body.oblecl );
    // declination
    m_body.declin = radtodeg * std::atan2( zequat, std::sqrt( xequat*xequat + yequat*yequat ) );
    // right ascension
    m_body.rascen = radtodeg * std::atan2( yequat, xequat );
*/
    // declination
    m_body.declin = radtodeg * std::asin( std::sin( m_body.oblecl * degtorad ) * std::sin( m_body.eclong * degtorad ) );

    // right ascension
    double top = std::cos( degtorad * m_body.oblecl ) * std::sin( degtorad * m_body.eclong );
    double bottom = std::cos( degtorad * m_body.eclong );

    m_body.rascen = clamp_circular( radtodeg * std::atan2( top, bottom ) );

    // Greenwich mean sidereal time
    m_observer.gmst = 6.697375 + 0.0657098242 * daynumber + m_observer.utime;

    m_observer.gmst -= 24.0 * (int)( m_observer.gmst / 24.0 );
    if( m_observer.gmst < 0.0 ) m_observer.gmst += 24.0;

    // local mean sidereal time
    m_observer.lmst = m_observer.gmst * 15.0 + m_observer.longitude;

    m_observer.lmst -= 360.0 * (int)( m_observer.lmst / 360.0 );
    if( m_observer.lmst < 0.0 ) m_observer.lmst += 360.0;

    // hour angle
    m_body.hrang = m_observer.lmst - m_body.rascen;

    if( m_body.hrang < -180.0 )	m_body.hrang += 360.0;	// (force it between -180 and 180 degrees)
    else if( m_body.hrang >  180.0 )	m_body.hrang -= 360.0;

    double cz;	// cosine of the solar zenith angle

    double tdatcd = std::cos( degtorad * m_body.declin );
    double tdatch = std::cos( degtorad * m_body.hrang );
    double tdatcl = std::cos( degtorad * m_observer.latitude );
    double tdatsd = std::sin( degtorad * m_body.declin );
    double tdatsl = std::sin( degtorad * m_observer.latitude );

    cz = tdatsd * tdatsl + tdatcd * tdatcl * tdatch;

    // (watch out for the roundoff errors)
    if( fabs( cz ) > 1.0 ) { cz >= 0.0 ? cz = 1.0 : cz = -1.0; }

    m_body.zenetr = std::acos( cz ) * radtodeg;
    m_body.elevetr = 90.0 - m_body.zenetr;
    refract();
}

void cSun::refract() {

	static double raddeg = 0.0174532925;					// converts from degrees to radians

	double prestemp;	// temporary pressure/temperature correction
	double refcor;		// temporary refraction correction
	double tanelev;		// tangent of the solar elevation angle

	// if the sun is near zenith, the algorithm bombs; refraction near 0.
	if( m_body.elevetr > 85.0 )
		refcor = 0.0;
	else {

		tanelev = tan( raddeg * m_body.elevetr );
		if( m_body.elevetr >= 5.0 )
			refcor	= 58.1 / tanelev
					- 0.07 / pow( tanelev, 3 )
					+ 0.000086 / pow( tanelev, 5 );
		else if( m_body.elevetr >= -0.575 )
			refcor  = 1735.0
					+ m_body.elevetr * ( -518.2 + m_body.elevetr *
					  ( 103.4 + m_body.elevetr * ( -12.79 + m_body.elevetr * 0.711 ) ) );
		else
			refcor  = -20.774 / tanelev;

		prestemp = ( m_observer.press * 283.0 ) / ( 1013.0 * ( 273.0 + m_observer.temp ) );
		refcor *= prestemp / 3600.0;
	}

	// refracted solar elevation angle
	m_body.elevref = m_body.elevetr + refcor;

	// refracted solar zenith angle
	m_body.zenref  = 90.0 - m_body.elevref;
}

void cSun::irradiance() {

	m_body.dayang = ( simulation::Time.year_day() - 1 ) * 360.0 / 365.0;
	double sd = std::sin( glm::radians( m_body.dayang ) ); // sine of the day angle
	double cd = std::cos( glm::radians( m_body.dayang ) ); // cosine of the day angle or delination
	m_body.erv = 1.000110 + 0.034221*cd + 0.001280*sd;
	double d2 = 2.0 * m_body.dayang;
	double c2 = std::cos( glm::radians( d2 ) );
	double s2 = std::sin( glm::radians( d2 ) );
	m_body.erv += 0.000719*c2 + 0.000077*s2;

	double solcon = 1367.0;									// Solar constant, 1367 W/sq m

	m_body.coszen  = std::cos( glm::radians( m_body.zenref ) );
	if( m_body.coszen > 0.0 ) {
		m_body.etrn = solcon * m_body.erv;
		m_body.etr  = m_body.etrn * m_body.coszen;
	}
	else {
		m_body.etrn = 0.0;
		m_body.etr  = 0.0;
	}
}
