pipeline {
    agent any

    stages {
        stage('Build Application Image') {
            steps {
                script {
                    echo 'Building the Lush application Docker image...'
                    // Build the image from the new Dockerfile and tag it
                    sh 'docker build -t lush-app:latest .'
                }
            }
        }
        stage('Compile Project in Container') {
            steps {
                script {
                    echo 'Compiling the Lush project inside the container...'
                    // Run the container to generate Makefiles and compile
                    // We mount the current directory to get the compiled artifacts back out
                    sh 'docker run --rm -v "$(pwd)":/app lush-app:latest'
                    sh 'docker run --rm -v "$(pwd)":/app lush-app:latest make'
                }
            }
        }
        stage('Run Lua 5.2 Compatibility Test') {
            steps {
                script {
                    echo 'Running the Lua 5.2 compatibility test script...'
                    // Create the test script file
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
                    // Run the container and execute the test script with the compiled binary
                    sh 'docker run --rm -v "$(pwd)":/app lush-app:latest ./bin/Debug/lush/lush test_52.lua'
                }
            }
        }
    }
    post {
        always {
            echo 'Pipeline finished.'
            // Clean up the created docker image to save space
            sh 'docker rmi lush-app:latest || true'
        }
    }
}
