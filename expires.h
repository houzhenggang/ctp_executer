#ifndef EXPIRES_H
#define EXPIRES_H

#include <QDateTime>

template <class T>
class Expires
{
protected:
    T m_value;
    QDateTime m_expire_time;

public:
    // Default ctor, which creates a Expires object with default T value and never expires
    Expires() : m_value(), m_expire_time() {}
    Expires(const T &v, const QDateTime &t = QDateTime()) : m_value(v), m_expire_time(t) {}

    T value() const;
    T originalValue() const;
    void setValue(const T &v);

    bool expired() const;
    void setExpire(const QDateTime &t);
};

/*!
 * \brief Expires<T>::value
 * Get the value, which may not exist if expired.
 * If expired return default T value, if not return m_value
 *
 * \return m_value or T()
 */
template <class T>
inline
T Expires<T>::value() const
{
    if (expired()) {
        return T();
    } else {
        return m_value;
    }
}

/*!
 * \brief Expires<T>::originalValue
 * Get the original value regardless of expired or not
 *
 * \return m_value
 */
template <class T>
inline
T Expires<T>::originalValue() const
{
    return m_value;
}

/*!
 * \brief Expires<T>::setValue
 * Set the stored value
 *
 * \param v value to store
 */
template <class T>
inline
void Expires<T>::setValue(const T &v)
{
    m_value = v;
}

/*!
 * \brief Expires<T>::expired
 * Check if the data has expired
 *
 * \return true if expired, otherwise false
 */
template <class T>
inline
bool Expires<T>::expired() const
{
    if (m_expire_time.isNull()) {
        // expire time is not set, regard as never expired
        return false;
    } else {
        if (m_expire_time.isValid()) {
            // expire time is valid, compare it with current time
            return m_expire_time <= QDateTime::currentDateTime();
        } else {
            // expire time is not valid, regard as expired
            return true;
        }
    }
}

/*!
 * \brief Expires<T>::setExpire
 * Set expire time
 *
 * \param t expire time
 */
template <class T>
inline
void Expires<T>::setExpire(const QDateTime &t)
{
    m_expire_time = t;
}

/*!
 * \brief make_expires
 * Returns a Expire<T> that contains value v and will expire at time t.
 *
 * \param v value
 * \param t expire time
 * \return Expires<T>(v, t)
 */
template <class T>
inline
Expires<T> make_expires(const T &v, const QDateTime &t = QDateTime())
{
    return Expires<T>(v, t);
}

/*!
 * \brief operator ==
 * Check if two Expires objects are equal.
 * This function requires the T type to have an implementation of operator==().
 *
 * \param e1 first oprand
 * \param e2 second oprand
 * \return true if e1 is equal to e2; otherwise returns false
 */
template <class T>
inline
bool operator==(const Expires<T> &e1, const Expires<T> &e2)
{
    if (e1.expired() || e2.expired()) {
        return false;
    } else {
        return e1.m_value == e2.m_value;
    }
}

#endif // EXPIRES_H
