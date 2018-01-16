#include "CPyObject.h"

CPyObject::CPyObject() : m_obj(nullptr)
{
	SetObject(nullptr);
}

CPyObject::CPyObject(PyObject *obj) : m_obj(nullptr)
{
	SetObject(obj);
}

CPyObject::CPyObject(const CPyObject &cobj) : m_obj(nullptr)
{
	SetObject(cobj.m_obj);
	Py_XINCREF(m_obj);
}

CPyObject::~CPyObject()
{
	Py_XDECREF(m_obj);
}

CPyObject CPyObject::Call(const CPyTuple &args) const
{
	return CheckObj(PyObject_CallObject(m_obj, args.m_obj));
}

const CPyObject CPyObject::GetAttr(const CPyString &name) const
{
	return CheckObj(PyObject_GetAttr(m_obj, name.m_obj));
}

bool CPyObject::SetAttr(const CPyString &name, const CPyObject &value)
{
	return Check3(PyObject_SetAttr(m_obj, name.m_obj, value.m_obj)) != -1;
}

bool CPyObject::DelAttr(const CPyString &name)
{
	return Check3(PyObject_DelAttr(m_obj, name.m_obj)) != -1;
}

const CPyObject CPyObject::GetItem(ssize_t pos) const
{
	return CheckObj(PyObject_GetItem(m_obj, CheckObj(PyLong_FromSsize_t(pos))));
}

ssize_t CPyObject::Len() const
{
	return Check3(PyObject_Length(m_obj));
}

CPyString CPyObject::Str() const
{
	return CheckObj(PyObject_Str(m_obj));
}

CPyObject CPyObject::Type() const
{
	return CheckObj(PyObject_Type(m_obj));
}

CPyIter CPyObject::GetIter() const
{
	return CheckObj(PyObject_GetIter(m_obj));
}

CPyTuple CPyObject::AsTuple() const
{
	Py_XINCREF(m_obj);
	return m_obj;
}

CPyList CPyObject::AsList() const
{
	Py_XINCREF(m_obj);
	return m_obj;
}

CPyString CPyObject::AsString() const
{
	Py_XINCREF(m_obj);
	return m_obj;
}

CPyLong CPyObject::AsLong() const
{
	Py_XINCREF(m_obj);
	return m_obj;
}

CPyFloat CPyObject::AsFloat() const
{
	Py_XINCREF(m_obj);
	return m_obj;
}

CPyNumber CPyObject::AsNumber() const
{
	Py_XINCREF(m_obj);
	return m_obj;
}

bool CPyObject::IsTuple() const
{
	return PyTuple_Check(m_obj);
}

bool CPyObject::IsList() const
{
	return PyList_Check(m_obj);
}

bool CPyObject::IsString() const
{
	return PyUnicode_Check(m_obj);
}

bool CPyObject::IsLong() const
{
	return PyLong_Check(m_obj);
}

bool CPyObject::IsFloat() const
{
	return PyFloat_Check(m_obj);
}

bool CPyObject::IsNumber() const
{
	return PyNumber_Check(m_obj) == 1;
}

PyObject *CPyObject::GetRawObject() const
{
	return m_obj;
}

const CPyObject &CPyObject::operator =(const CPyObject& cobj)
{
	CopyObject(cobj.m_obj);
	return *this;
}

bool CPyObject::operator ==(const CPyObject& cobj) const
{
	return Check3(PyObject_RichCompareBool(m_obj, cobj.m_obj, Py_EQ)) == 1; // TODO: Error handling?
}

bool CPyObject::operator !=(const CPyObject& cobj) const
{
	return Check3(PyObject_RichCompareBool(m_obj, cobj.m_obj, Py_EQ)) == 0;
}

const CPyObject CPyObject::operator [](ssize_t pos) const
{
	return GetItem(pos);
}

CPyObject::operator CPyTuple() const
{
	return AsTuple();
}

CPyObject::operator CPyList() const
{
	return AsList();
}

CPyObject::operator CPyString() const
{
	return AsString();
}

CPyObject::operator CPyLong() const
{
	return AsLong();
}

CPyObject::operator CPyFloat() const
{
	return AsFloat();
}

CPyObject::operator CPyNumber() const
{
	return AsNumber();
}

std::ostream &operator <<(std::ostream &stream, const CPyObject &cobj)
{
	return stream << (std::string)cobj.Str();
}

int CPyObject::Check2(int val) const
{
	if (val == 0)
		CheckErr();

	return val;
}

Py_ssize_t CPyObject::Check3(Py_ssize_t val) const
{
	if (val == -1)
		CheckErr();

	return val;
}

PyObject *CPyObject::CheckObj(PyObject *obj) const
{
	if (!obj) CheckErr();

	return obj;
}

void CPyObject::CheckErr() const
{
	PyObject *err_obj = PyErr_Occurred();

	if (err_obj)
	{
		Py_XINCREF(err_obj);
		PyErr_Print(); // TODO: STFU
		PyErr_Clear();
		throw CPyException(CPyObject(err_obj).Str());
	}
}

void CPyObject::SetObject(PyObject *obj)
{
	if (obj)
	{
		Py_XDECREF(m_obj);
		m_obj = obj;
	}
	else
	{
		Py_INCREF(Py_None);
		m_obj = Py_None;
	}
}

void CPyObject::CopyObject(PyObject *obj)
{
	if (obj)
	{
		Py_XDECREF(m_obj);
		m_obj = obj;
	}
	else
	{
		m_obj = Py_None;
	}

	Py_XINCREF(m_obj);
}

CPyException::CPyException(const CPyString &text) : m_text(text)
{
}

const std::string &CPyException::GetText() const
{
	return m_text;
}

CPyModule::CPyModule(const CPyString &name)
{
	PyObject *module_obj = CheckObj(PyImport_Import(name.GetRawObject()));

	if (!module_obj)
		throw CPyException("not a module");

	m_obj = module_obj;
}

CPyModule::CPyModule(PyObject *obj) : CPyObject(obj)
{
	if (!PyModule_Check(m_obj))
		throw CPyException("not a module");
}

void CPyModule::Reload()
{
	m_obj = CheckObj(PyImport_ReloadModule(m_obj));
}

CPyIter::CPyIter(PyObject *obj) : CPyObject(obj)
{
	if (!PyIter_Check(m_obj)) throw CPyException("not an iterator");

	m_next = PyIter_Next(m_obj);
}

bool CPyIter::HasNext() const
{
	return m_next != nullptr;
}

CPyObject CPyIter::Next()
{
	PyObject *cur = m_next;
	
	m_next = CheckObj(PyIter_Next(m_obj));

	return cur;
}

CPyTuple::CPyTuple()
{
	m_obj = CheckObj(PyTuple_New(0));
}

CPyTuple::CPyTuple(ssize_t size)
{
	m_obj = CheckObj(PyTuple_New(size));
}

CPyTuple::CPyTuple(PyObject *obj) : CPyObject(obj)
{
	if (!IsTuple())
		throw CPyException("not a tuple");
}

CPyObject CPyTuple::GetSlice(ssize_t low, ssize_t high) const
{
	return CheckObj(PyTuple_GetSlice(m_obj, low, high));
}

CPyObject CPyTuple::GetItem(ssize_t pos) const
{
	PyObject *item_obj = CheckObj(PyTuple_GetItem(m_obj, pos));

	Py_XINCREF(item_obj);
	return item_obj;
}

bool CPyTuple::SetItem(ssize_t pos, const CPyObject& cobj)
{
	PyObject *item_obj = cobj.GetRawObject();

	Py_XINCREF(item_obj);
	return PyTuple_SetItem(m_obj, pos, item_obj) == 0;
}

ssize_t CPyTuple::Size() const
{
	return PyTuple_Size(m_obj);
}

CPyList::CPyList()
{
	m_obj = CheckObj(PyList_New(0));
}

CPyList::CPyList(ssize_t size)
{
	m_obj = CheckObj(PyList_New(size));
}

CPyList::CPyList(PyObject *obj) : CPyObject(obj)
{
	if (!IsList())
		throw CPyException("not a list");
}

void CPyList::Append(const CPyObject& cobj)
{
	Check3(PyList_Append(m_obj, cobj.GetRawObject()));
}

CPyObject CPyList::GetSlice(ssize_t low, ssize_t high) const
{
	return CheckObj(PyList_GetSlice(m_obj, low, high));
}

CPyObject CPyList::GetItem(ssize_t pos) const
{
	PyObject *item_obj = CheckObj(PyList_GetItem(m_obj, pos));

	Py_XINCREF(item_obj);
	return item_obj;
}

bool CPyList::SetItem(ssize_t pos, const CPyObject& cobj)
{
	PyObject *item_obj = cobj.GetRawObject();

	Py_XINCREF(item_obj);
	return PyList_SetItem(m_obj, pos, item_obj) == 0;
}

ssize_t CPyList::Size() const
{
	return PyList_Size(m_obj);
}

CPyString::CPyString()
{
	m_obj = CheckObj(PyUnicode_FromString(""));
}

CPyString::CPyString(const char *str)
{
	m_obj = CheckObj(PyUnicode_FromString(str));
}

CPyString::CPyString(const std::string &str)
{
	m_obj = CheckObj(PyUnicode_FromString(str.c_str()));
}

CPyString::CPyString(PyObject *obj) : CPyObject(obj)
{
	if (!IsString())
		throw CPyException("not a string");
}

CPyString::operator std::string() const
{
	PyObject *bytes_obj = CheckObj(PyUnicode_AsUTF8String(m_obj));

	if (!bytes_obj) return "";

	return PyBytes_AsString(bytes_obj);
}

CPyLong::CPyLong()
{
	m_obj = CheckObj(PyLong_FromLong(0L));
}

CPyLong::CPyLong(long long val)
{
	m_obj = CheckObj(PyLong_FromLongLong(val));
}

CPyLong::CPyLong(unsigned long long val)
{
	m_obj = CheckObj(PyLong_FromUnsignedLongLong(val));
}

CPyLong::CPyLong(PyObject *obj) : CPyObject(obj)
{
	if (!IsLong()) throw CPyException("not a long");
}

CPyLong::operator long() const
{
	return PyLong_AsLong(m_obj);
}

CPyLong::operator long long() const
{
	return PyLong_AsLongLong(m_obj);
}

CPyLong::operator unsigned long() const
{
	return PyLong_AsUnsignedLong(m_obj);
}

CPyLong::operator unsigned long long() const
{
	return PyLong_AsUnsignedLongLong(m_obj);
}

CPyFloat::CPyFloat()
{
	m_obj = CheckObj(PyFloat_FromDouble(0.0));
}

CPyFloat::CPyFloat(double val)
{
	m_obj = CheckObj(PyFloat_FromDouble(val));
}

CPyFloat::CPyFloat(PyObject *obj) : CPyObject(obj)
{
	if (PyLong_Check(m_obj)) m_obj = CheckObj(PyFloat_FromDouble(PyLong_AsDouble(m_obj)));

	if (!IsFloat()) throw CPyException("not a float");
}

CPyFloat::operator double() const
{
	return PyFloat_AsDouble(m_obj);
}

CPyNumber::CPyNumber()
{
	m_obj = CheckObj(PyLong_FromLong(0L));
}

CPyNumber::CPyNumber(ssize_t val)
{
	m_obj = CheckObj(PyLong_FromSsize_t(val));
}

CPyNumber::CPyNumber(PyObject *obj) : CPyObject(obj)
{
	if (!PyNumber_Check(m_obj)) throw CPyException("not a numbers");
}

CPyNumber CPyNumber::operator >>(int shift)
{
	return PyNumber_Rshift(m_obj, CheckObj(PyLong_FromLong(shift)));
}

CPyNumber::operator unsigned long long() const
{
	return CPyLong(PyLong_AsUnsignedLongLongMask((PyNumber_Long(m_obj))));
}
