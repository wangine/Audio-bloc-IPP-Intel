#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <charconv>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <map>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <xmmintrin.h>
namespace entropy::native {
static_assert(sizeof(void*)==8 && sizeof(double)==8 && std::numeric_limits<double>::is_iec559);
static_assert(std::atomic<float>::is_always_lock_free && std::atomic<double>::is_always_lock_free);
static_assert(std::atomic<bool>::is_always_lock_free && std::atomic<uint32_t>::is_always_lock_free && std::atomic<uint64_t>::is_always_lock_free);
class String {
 std::string value;
public:
 String()=default; String(const char* s):value(s?s:""){} String(std::string s):value(std::move(s)){} String(int n):value(std::to_string(n)){}
 const char* toRawUTF8()const noexcept{return value.c_str();} std::string toStdString()const{return value;} bool isEmpty()const noexcept{return value.empty();}
 String replace(const String& from,const String& to)const{auto s=value;auto f=from.value;size_t pos=0;if(f.empty())return s;while((pos=s.find(f,pos))!=std::string::npos){s.replace(pos,f.size(),to.value);pos+=to.value.size();}return s;}
 friend String operator+(const String&a,const String&b){return a.value+b.value;} friend bool operator==(const String&a,const String&b){return a.value==b.value;} friend bool operator<(const String&a,const String&b){return a.value<b.value;}
};
class StringArray {
 std::vector<String> values;
public: StringArray()=default;StringArray(std::initializer_list<String> a):values(a){} int size()const{return (int)values.size();}void add(String s){values.push_back(std::move(s));}const String&operator[](int i)const{return values[(size_t)i];}auto begin()const{return values.begin();}auto end()const{return values.end();}
};
template<class T> constexpr T jlimit(T lo,T hi,T x)noexcept{return std::max(lo,std::min(hi,x));}
template<class T> constexpr T jmin(T a,T b)noexcept{return std::min(a,b);} template<class T>constexpr T jmax(T a,T b)noexcept{return std::max(a,b);}
template<class T>struct MathConstants{static constexpr T pi=std::numbers::pi_v<T>,twoPi=2*pi;};
struct Decibels {template<class T>static T decibelsToGain(T db,T minusInfinity=T(-100)){return db>minusInfinity?std::pow(T(10),db/T(20)):T(0);}template<class T>static T gainToDecibels(T gain,T minusInfinity=T(-100)){return gain>T(0)?std::max(minusInfinity,T(20)*std::log10(gain)):minusInfinity;}};
struct Time{static double getMillisecondCounterHiRes(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}static int64_t currentTimeMillis(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();}};
struct ScopedNoDenormals{unsigned previous=_mm_getcsr();ScopedNoDenormals(){_mm_setcsr(previous|0x8040);}~ScopedNoDenormals(){_mm_setcsr(previous);}};
struct MidiBuffer{};
namespace ValueSmoothingTypes{struct Linear{};}
template<class T,class Mode=ValueSmoothingTypes::Linear>class SmoothedValue {
 T current{},target{},step{};int countdown=0,steps=0;
public:
 void reset(double rate,double seconds){steps=(int)std::floor(rate*seconds);countdown=0;current=target;}
 void setCurrentAndTargetValue(T v)noexcept{current=target=v;countdown=0;step=0;}
 void setTargetValue(T v)noexcept{if(v==target)return;target=v;if(steps<=0){setCurrentAndTargetValue(v);return;}countdown=steps;step=(target-current)/(T)countdown;}
 T getNextValue()noexcept{if(countdown>0){--countdown;if(countdown>0)current+=step;else current=target;}return current;}
 T skip(int n)noexcept{if(n>=countdown){setCurrentAndTargetValue(target);return current;}current+=step*(T)n;countdown-=n;return current;}
 T getCurrentValue()const noexcept{return current;}T getTargetValue()const noexcept{return target;}bool isSmoothing()const noexcept{return countdown>0;}
};
// Owned storage is allocated by prepare/control calls only. Views carry inline pointers.
template<class T>class AudioBuffer {
 std::vector<T> storage;std::array<T*,16> pointers{};int channels=0,samples=0,capacity=0;
public:
 AudioBuffer()=default;AudioBuffer(int c,int n){setSize(c,n);}AudioBuffer(T*const* p,int c,int n):channels(c),samples(n){if(c<0||c>16)throw std::invalid_argument("channels");for(int i=0;i<c;++i)pointers[i]=p[i];}
 AudioBuffer(const AudioBuffer&)=delete;AudioBuffer&operator=(const AudioBuffer&)=delete;AudioBuffer(AudioBuffer&&)=default;AudioBuffer&operator=(AudioBuffer&&)=default;
 void setSize(int c,int n,bool=false,bool=false,bool=false){if(c<0||c>16||n<0)throw std::invalid_argument("buffer dimensions");if(c!=channels||n>capacity){capacity=n;storage.assign((size_t)c*n,T{});}channels=c;samples=n;for(int i=0;i<c;++i)pointers[i]=storage.data()+(size_t)i*capacity;}
 AudioBuffer slice(int first,int count)noexcept{AudioBuffer result;result.channels=std::min(count,std::max(0,channels-first));result.samples=samples;for(int i=0;i<result.channels;++i)result.pointers[i]=pointers[first+i];return result;}
 T*const*getArrayOfWritePointers()const noexcept{return pointers.data();}
 int getNumChannels()const noexcept{return channels;}int getNumSamples()const noexcept{return samples;}T*getWritePointer(int c,int offset=0)noexcept{return pointers[c]+offset;}const T*getReadPointer(int c,int offset=0)const noexcept{return pointers[c]+offset;}
 void clear()noexcept{for(int i=0;i<channels;++i)clear(i,0,samples);}void clear(int c,int offset,int n)noexcept{std::fill_n(pointers[c]+offset,n,T{});}
 T getSample(int c,int n)const noexcept{return pointers[c][n];}void setSample(int c,int n,T value)noexcept{pointers[c][n]=value;}
 void copyFrom(int dc,int dest,const AudioBuffer&src,int sc,int offset,int n)noexcept{std::copy_n(src.pointers[sc]+offset,n,pointers[dc]+dest);}void copyFrom(int dc,int dest,const T*src,int n)noexcept{std::copy_n(src,n,pointers[dc]+dest);}
 T getMagnitude(int c,int start,int n)const noexcept{T peak{};for(int i=0;i<n;++i)peak=std::max(peak,std::abs(pointers[c][start+i]));return peak;}
 void applyGain(int c,int start,int n,T gain)noexcept{for(int i=0;i<n;++i)pointers[c][start+i]*=gain;}
};
class MemoryBlock {
 std::vector<uint8_t> bytes;
public:void setSize(size_t n,bool=false){bytes.resize(n);}void*getData()noexcept{return bytes.data();}const void*getData()const noexcept{return bytes.data();}size_t getSize()const noexcept{return bytes.size();}void append(const void*p,size_t n){const auto*b=(const uint8_t*)p;bytes.insert(bytes.end(),b,b+n);}
};
template<class T>struct NormalisableRange {
 T start{},end{1},interval{},skew{1},encodingOffset{};bool nearestEven=false; NormalisableRange()=default;NormalisableRange(T a,T b,T step=T{},T power=T(1),bool integer=false):start(a),end(b),interval(step),skew(power),nearestEven(integer){}
 T snapToLegalValue(T v)const noexcept{if(nearestEven){v=jlimit(start,end,v);const auto lower=std::floor(v);const auto fraction=v-lower;return fraction>T(.5)?lower+T(1):fraction<T(.5)?lower:(std::fmod(lower,T(2))==T{}?lower:lower+T(1));}if(interval>T{}){const auto origin=start-encodingOffset;v=origin+interval*std::floor((v-origin)/interval+T(0.5));}return jlimit(start,end,v);}
 T convertTo0to1(T v)const noexcept{v=jlimit(start,end,v);const auto origin=start-encodingOffset;auto n=jlimit(T{},T(1),(v-origin)/(end-origin));return skew==T(1)?n:std::pow(n,skew);}
 T convertFrom0to1(T n)const noexcept{n=jlimit(T{},T(1),n);if(skew!=T(1))n=std::exp(std::log(n)/skew);const auto origin=start-encodingOffset;return jlimit(start,end,origin+(end-origin)*n);}
};
// Retain the accepted positive FP32 control round-trip while narrowing the
// legal millisecond range. This offset is parameter encoding only; legacy
// control rounding is retained, with no Global Base or added signal delay.
inline NormalisableRange<float> nonnegativeDelayRange() {
 NormalisableRange<float> range(0.0f,800.0f,0.001f);range.encodingOffset=800.0f;return range;
}
struct ParameterID{String id;int version;};
inline bool approximatelyEqual(float a,float b)noexcept{const auto diff=std::abs(a-b);return diff<=std::numeric_limits<float>::min()||diff<=std::numeric_limits<float>::epsilon()*std::max(std::abs(a),std::abs(b));}
class RangedAudioParameter {
 std::atomic<float> storedValue;bool published=false;
public:
 String paramID,name,label;NormalisableRange<float> range;std::atomic<float>raw;float defaultValue;
 RangedAudioParameter(ParameterID id,String n,NormalisableRange<float> r,float d,String units={}):storedValue(d),paramID(id.id),name(n),label(units),range(r),raw(r.convertFrom0to1(r.convertTo0to1(r.snapToLegalValue(d)))),defaultValue(d){}virtual ~RangedAudioParameter()=default;
 const NormalisableRange<float>&getNormalisableRange()const noexcept{return range;}float convertTo0to1(float v)const noexcept{return range.convertTo0to1(range.snapToLegalValue(v));}float convertFrom0to1(float n)const noexcept{return range.snapToLegalValue(range.convertFrom0to1(n));}
 void setValueNotifyingHost(float n)noexcept{storedValue.store(convertFrom0to1(n),std::memory_order_relaxed);const auto mirror=convertFrom0to1(getValue());if(!published||!approximatelyEqual(raw.load(std::memory_order_relaxed),mirror))raw.store(mirror,std::memory_order_relaxed);published=true;}float getValue()const noexcept{return convertTo0to1(storedValue.load(std::memory_order_relaxed));}float getDefaultValue()const noexcept{return convertTo0to1(defaultValue);}String getName(int=1024)const{return name;}String getLabel()const{return label;}
 virtual int getNumSteps()const noexcept{return range.interval>0?(int)((range.end-range.start)/range.interval)+1:0x7fffffff;}virtual bool isDiscrete()const noexcept{return false;}virtual String getText(float normalized,int)const{return String(std::to_string(convertFrom0to1(normalized)));}
};
class AudioParameterFloat:public RangedAudioParameter{public:using RangedAudioParameter::RangedAudioParameter;};
class AudioParameterInt:public RangedAudioParameter{public:AudioParameterInt(ParameterID id,String name,int a,int b,int d):RangedAudioParameter(id,name,{(float)a,(float)b,1,1,true},(float)d){}bool isDiscrete()const noexcept override{return true;}};
class AudioParameterBool:public RangedAudioParameter{public:AudioParameterBool(ParameterID id,String name,bool d):RangedAudioParameter(id,name,{0,1,1},d?1.f:0.f){}bool isDiscrete()const noexcept override{return true;}String getText(float v,int)const override{return v>=0.5f?"On":"Off";}};
class AudioParameterChoice:public RangedAudioParameter{public:StringArray choices;AudioParameterChoice(ParameterID id,String name,StringArray c,int d):RangedAudioParameter(id,name,{0,(float)c.size()-1,1,1,true},(float)d),choices(std::move(c)){}bool isDiscrete()const noexcept override{return true;}String getText(float v,int)const override{return choices[(int)convertFrom0to1(v)];}};
class Property {
 String text;
public:Property()=default;Property(String s):text(std::move(s)){}Property(const char*s):text(s){}Property(float v):text(format(v)){}Property(double v):text(format(v)){}Property(int v):text(std::to_string(v)){}Property(int64_t v):text(std::to_string(v)){}
 static String format(double v){char b[64];auto r=std::to_chars(b,b+64,v,std::chars_format::general,std::numeric_limits<double>::max_digits10);return std::string(b,r.ptr);}
 String toString()const{return text;}operator float()const{float v=0;auto s=text.toStdString();std::from_chars(s.data(),s.data()+s.size(),v);return v;}
};
// Small control-thread XML tree, preserving unknown attributes and child nodes.
class XmlNode {
 struct Data{String type;std::map<String,Property>attrs;std::vector<XmlNode>children;};std::shared_ptr<Data>d;
public:
 XmlNode()=default;XmlNode(String type):d(std::make_shared<Data>()){d->type=std::move(type);}bool isValid()const{return !!d;}String getType()const{return d?d->type:String();}bool hasType(String t)const{return d&&d->type==t;}bool hasTagName(String t)const{return hasType(t);}
 void setProperty(String key,Property value,void*){d->attrs[key]=std::move(value);}bool hasProperty(String key)const{return d&&d->attrs.count(key)>0;}Property getProperty(String key)const{if(d){auto i=d->attrs.find(key);if(i!=d->attrs.end())return i->second;}return {};}
 void removeProperty(String key,void*){if(d)d->attrs.erase(key);}
 int getNumChildren()const{return d?(int)d->children.size():0;}XmlNode getChild(int i)const{return d->children[(size_t)i];}XmlNode getChildWithName(String type)const{if(d)for(auto&c:d->children)if(c.hasType(type))return c;return {};}
 void addChild(XmlNode n,int,void*){d->children.push_back(std::move(n));}void removeChild(XmlNode n,void*){auto&v=d->children;v.erase(std::remove_if(v.begin(),v.end(),[&](auto&c){return c.d==n.d;}),v.end());}
 std::unique_ptr<XmlNode> createXml()const{return std::make_unique<XmlNode>(*this);}static XmlNode fromXml(const XmlNode&n){return n;}
 std::string serialize()const;static std::unique_ptr<XmlNode>parse(std::string_view xml);
};
using Identifier=String;using int64=int64_t;
void copyXmlToBinary(const XmlNode&,MemoryBlock&);std::unique_ptr<XmlNode>getXmlFromBinary(const void*,int);
class ParameterStore {
 std::vector<std::unique_ptr<RangedAudioParameter>>parameters;
public:
 using ParameterLayout=std::vector<std::unique_ptr<RangedAudioParameter>>;XmlNode state{"PARAMETERS"};
 explicit ParameterStore(ParameterLayout p):parameters(std::move(p)){
  // Connecting an initially empty legacy state publishes a missing PARAM's
  // snapped default only when it differs from the adapter's unsnapped mirror.
  // This matters for signed delay ranges close to zero; skewed defaults may
  // legitimately retain their unsnapped value within the legacy epsilon.
  for(auto& parameter:parameters){
   const float value=parameter->convertFrom0to1(parameter->getDefaultValue());
   if(!approximatelyEqual(value,parameter->raw.load()))parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
   XmlNode node("PARAM");node.setProperty("id",parameter->paramID,nullptr);node.setProperty("value",parameter->raw.load(),nullptr);state.addChild(node,-1,nullptr);
  }
 }
 RangedAudioParameter*getParameter(String id)const noexcept{for(auto&p:parameters)if(p->paramID==id)return p.get();return nullptr;}std::atomic<float>*getRawParameterValue(String id)const noexcept{auto*p=getParameter(id);return p?&p->raw:nullptr;}
 XmlNode copyState(){for(auto&p:parameters){XmlNode n;for(int i=0;i<state.getNumChildren();++i){auto c=state.getChild(i);if(c.hasType("PARAM")&&c.getProperty("id").toString()==p->paramID){n=c;break;}}if(!n.isValid()){n=XmlNode("PARAM");n.setProperty("id",p->paramID,nullptr);state.addChild(n,-1,nullptr);}n.setProperty("value",p->raw.load(),nullptr);}return state;}
 void replaceState(XmlNode n){
  state=std::move(n);
  for(auto& p:parameters){
   XmlNode parameterNode;
   for(int i=0;i<state.getNumChildren();++i){auto child=state.getChild(i);if(child.hasType("PARAM")&&child.getProperty("id").toString()==p->paramID){parameterNode=child;break;}}
   // Legacy partial states restore absent controls to their declared default.
   // Unknown root attributes and child nodes remain in the supplied state.
   const float value=parameterNode.isValid()?(float)parameterNode.getProperty("value"):p->convertFrom0to1(p->getDefaultValue());
   if(!approximatelyEqual(value,p->raw.load()))p->setValueNotifyingHost(p->convertTo0to1(value));
   if(!parameterNode.isValid()){parameterNode=XmlNode("PARAM");parameterNode.setProperty("id",p->paramID,nullptr);parameterNode.setProperty("value",p->raw.load(),nullptr);state.addChild(parameterNode,-1,nullptr);}
  }
 }
};
}
