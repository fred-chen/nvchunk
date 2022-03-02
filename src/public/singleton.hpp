template <class T>
class Singleton
{
public:
    static T& instance()
    {
        static T inst;
        return inst;
    }

    Singleton(Singleton<T> const &)         = delete;
    void operator=(Singleton<T> const &)    = delete;
protected:
    Singleton() = default;
    ~Singleton() = default;
};
