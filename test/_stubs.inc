	void Push( int iStackPos ) override {}
	void SetField( int iStackPos, const char* strName ) override {}
	void SetMetaTable( int iStackPos ) override {}
	bool GetMetaTable( int i ) override { return 0; }
	void Call( int iArgs, int iResults ) override {}
	int Equal( int iA, int iB ) override { return 0; }
	int RawEqual( int iA, int iB ) override { return 0; }
	void Insert( int iStackPos ) override {}
	void Remove( int iStackPos ) override {}
	int Next( int iStackPos ) override { return 0; }
	void* NewUserdata( unsigned int iSize ) override { return 0; }
	void ThrowError( const char* strError ) override {}
	void CheckType( int iStackPos, int iType ) override {}
	void ArgError( int iArgNum, const char* strMessage ) override {}
	bool GetBool( int iStackPos = -1 ) override { return 0; }
	CFunc GetCFunction( int iStackPos = -1 ) override { return 0; }
	void* GetUserdata( int iStackPos = -1 ) override { return 0; }
	void PushBool( bool val ) override {}
	void PushCFunction( CFunc val ) override {}
	void PushCClosure( CFunc val, int iVars ) override {}
	void PushUserdata( void* ) override {}
	int ReferenceCreate() override { return 0; }
	void ReferenceFree( int i ) override {}
	const char* GetTypeName( int iType ) override { return 0; }
	void CreateMetaTableType( const char* strName, int iType ) override {}
	const char* CheckString( int iStackPos = -1 ) override { return 0; }
	double CheckNumber( int iStackPos = -1 ) override { return 0; }
	int ObjLen( int iStackPos = -1 ) override { return 0; }
	const QAngle& GetAngle( int iStackPos = -1 ) override { static char b[1]{}; return *(const QAngle*)b; }
	const Vector& GetVector( int iStackPos = -1 ) override { static char b[1]{}; return *(const Vector*)b; }
	void PushAngle( const QAngle& val ) override {}
	void PushVector( const Vector& val ) override {}
	void SetState( lua_State* L ) override {}
	int CreateMetaTable( const char* strName ) override { return 0; }
	bool PushMetaTable( int iType ) override { return 0; }
	void PushUserType( void* data, int iType ) override {}
	void SetUserType( int iStackPos, void* data ) override {}
