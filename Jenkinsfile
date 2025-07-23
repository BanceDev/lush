pipeline {
    agent any

    stages {
        stage('Build Application Image') {
            steps {
                script {
                    echo 'Pre-build diagnostics...'
                    sh 'pwd'
                    sh 'ls -la'
                    sh 'find . -name "premake5.lua" -type f'
                    
                    echo 'Initializing Git submodules...'
                    sh 'git submodule update --init --recursive'
                    
                    echo 'Post-submodule diagnostics...'
                    sh 'ls -la'
                    sh 'find . -name "premake5.lua" -type f'
                    sh 'ls -lR | head -50'  // Show directory structure

                    echo 'Building the Lush application Docker image...'
                    sh 'docker build -t lush-app:latest .'
                }
            }
        }
        stage('Compile Project in Container') {
            steps {
                script {
                    echo 'Container diagnostics...'
                    sh 'docker run --rm -v "$(pwd)":/app lush-app:latest pwd'
                    sh 'docker run --rm -v "$(pwd)":/app lush-app:latest ls -la'
                    sh 'docker run --rm -v "$(pwd)":/app lush-app:latest find . -name "premake5.lua" -type f'
                    sh 'docker run --rm -v "$(pwd)":/app lush-app:latest ls -la /app'
                    
                    echo 'Compiling the Lush project inside the container...'
                    // First run premake to generate build files
                    sh 'docker run --rm -v "$(pwd)":/app lush-app:latest premake5 gmake2'
                    // Then run make to compile
                    sh 'docker run --rm -v "$(pwd)":/app lush-app:latest make'
                }
            }
        }
        stage('Run Lua 5.2 Compatibility Test') {
            steps {
                script {
                    echo 'Pre-test diagnostics...'
                    sh 'docker run --rm -v "$(pwd)":/app lush-app:latest ls -la bin/'
                    sh 'docker run --rm -v "$(pwd)":/app lush-app:latest find . -name "lush" -type f'
                    
                    echo 'Running the Lua 5.2 compatibility test script...'
                    sh '''
                    cat <<'EOF' > test_52.lua
                    -- Lua 5.2-specific features
                    print("--- Running Lua 5.2 Compatibility Test ---")
                    local _ENV = { print = print, x = 123 } -- lexical environments
                    function show_x()
                        print("x =", x)
                    end
                    show_x()
                    -- Bitwise ops (added in 5.2)
                    local a, b = 0x5, 0x3
                    print("Bitwise AND:", a & b)
                    -- load() replaces loadstring() (binary and text)
                    local f = load("return 10 + 20")
                    print("Loaded result:", f())
                    -- table.pack / table.unpack
                    local t = table.pack(1, 2, 3, nil, 5)
                    print("Packed length:", t.n)
                    print("Unpacked values:", table.unpack(t, 1, t.n))
                    print("--- Test Complete ---")
                    EOF
                    '''
                    sh 'docker run --rm -v "$(pwd)":/app lush-app:latest ./bin/Debug/lush/lush test_52.lua'
                }
            }
        }
    }
    post {
        always {
            echo 'Pipeline finished.'
            sh 'docker rmi lush-app:latest || true'
        }
    }
}